# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-guided differential privacy helpers — adaptive noise + clipping (lab, not a DP proof).

Low-σ layers accept **more** Gaussian perturbation; high-σ layers receive **less** noise so
precision is preserved. Combines with :class:`cos.sigma_federated.SigmaDifferentialPrivacy`
for non-adaptive baselines; see ``docs/CLAIM_DISCIPLINE.md``.

``python/cos/sigma_gate.h`` is **not** modified here.
"""
from __future__ import annotations

import copy
import hashlib
import math
import random
import re
from typing import Any, Dict, List, Mapping, Optional, Sequence

__all__ = ["SigmaPrivacy"]

GradMap = Dict[str, List[float]]

_EMAIL_RX = re.compile(r"\b[\w.+-]+@[\w.-]+\.\w{2,}\b")
_PHONE_RX = re.compile(r"\b\+?\d[\d\s\-]{7,}\d\b")


class SigmaPrivacy:
    """DP-style noise and clipping with σ-conditioned scaling; append-only audit rows."""

    def __init__(self, *, rng: Optional[random.Random] = None) -> None:
        self._rng = rng or random.Random()
        self._audit: List[Dict[str, Any]] = []

    def audit_trail(self) -> List[Dict[str, Any]]:
        return list(self._audit)

    def _log(self, op: str, payload: Dict[str, Any]) -> None:
        row = {"op": op, **payload}
        self._audit.append(row)

    @staticmethod
    def _digest_vec(layer: str, vec: Sequence[float]) -> str:
        h = hashlib.sha256()
        h.update(layer.encode())
        h.update(b"|")
        for x in vec[:32]:
            h.update(f"{x:.8f};".encode())
        h.update(str(len(vec)).encode())
        return h.hexdigest()[:24]

    def _per_layer_sigma(self, gate: Any, layer: str, vec: Sequence[float]) -> float:
        digest = self._digest_vec(layer, vec)
        return float(gate.compute_sigma(None, None, f"privacy_adapt:{layer}", digest))

    def gradient_clip(
        self,
        gradients: GradMap,
        max_norm: float,
        gate: Any,
        *,
        layer_sigma_override: Optional[Mapping[str, float]] = None,
    ) -> GradMap:
        """L2 clip the joint vector; per-layer σ tightens clip (high σ → smaller effective ball)."""
        if max_norm <= 0:
            raise ValueError("max_norm must be positive")
        g2 = copy.deepcopy(gradients)
        sigma_map: Dict[str, float] = {}
        for layer, vec in g2.items():
            sig = (
                float(layer_sigma_override[layer])
                if layer_sigma_override and layer in layer_sigma_override
                else self._per_layer_sigma(gate, layer, vec)
            )
            sigma_map[layer] = sig
        # Tighter global clip when average layer stress is high
        avg_s = sum(sigma_map.values()) / max(len(sigma_map), 1)
        scale_cap = max_norm / (1.0 + 0.75 * avg_s)

        flat: List[float] = []
        spans: List[tuple[str, int, int]] = []
        for layer, vec in g2.items():
            start = len(flat)
            flat.extend(float(x) for x in vec)
            spans.append((layer, start, len(flat)))

        nrm = math.sqrt(sum(x * x for x in flat))
        if nrm <= scale_cap or nrm <= 0.0:
            self._log("gradient_clip", {"sigma_map": sigma_map, "norm": nrm, "clipped": False})
            return g2
        sc = scale_cap / nrm
        clipped = [x * sc for x in flat]
        out: GradMap = {}
        for layer, a, b in spans:
            out[layer] = clipped[a:b]
        self._log(
            "gradient_clip",
            {"sigma_map": sigma_map, "norm_before": nrm, "clipped": True, "scale": sc},
        )
        return out

    def dp_noise(
        self,
        gradients: GradMap,
        epsilon: float,
        gate: Any,
        *,
        sensitivity: float = 1.0,
        layer_sigma_override: Optional[Mapping[str, float]] = None,
    ) -> GradMap:
        """Add Gaussian noise; scale ↑ when layer σ is low (more robust), ↓ when σ is high."""
        if epsilon <= 0:
            raise ValueError("epsilon must be positive")
        base_scale = float(sensitivity) / float(epsilon)
        out: GradMap = {}
        sigma_map: Dict[str, float] = {}
        for layer, vec in gradients.items():
            sig = (
                float(layer_sigma_override[layer])
                if layer_sigma_override and layer in layer_sigma_override
                else self._per_layer_sigma(gate, layer, vec)
            )
            sig = min(1.0, max(0.0, sig))
            # Low σ → factor up to ~1.85×; high σ → factor down toward ~0.2×
            factor = 0.2 + 1.65 * (1.0 - sig)
            std = base_scale * factor
            sigma_map[layer] = sig
            out[layer] = [float(x) + self._rng.gauss(0.0, std) for x in vec]
        self._log(
            "dp_noise",
            {"epsilon": epsilon, "sensitivity": sensitivity, "layer_sigma": sigma_map},
        )
        return out

    @staticmethod
    def privacy_budget(epsilon: float, delta: float, n_steps: int) -> Dict[str, float]:
        """Rough composition ledger (lab math — not a full RDP accountant)."""
        if n_steps < 1:
            raise ValueError("n_steps must be >= 1")
        if delta <= 0:
            raise ValueError("delta must be positive")
        # Simple sqrt composition upper sketch
        spent = float(epsilon) * math.sqrt(float(n_steps))
        remaining = max(0.0, float(epsilon) - spent / max(1.0, math.log(1.0 + float(n_steps))))
        return {
            "epsilon_budget": float(epsilon),
            "delta": float(delta),
            "steps": int(n_steps),
            "approx_spent_upper": round(spent, 6),
            "approx_remaining": round(remaining, 6),
            "disclaimer": "Lab approximation; use a formal accountant for production DP claims.",
        }

    def pii_redact(self, text: str) -> str:
        """Redact email / phone patterns before storage or uplink (heuristic)."""

        def _email_sub(m: re.Match[str]) -> str:
            return "[EMAIL_REDACTED]"

        def _phone_sub(m: re.Match[str]) -> str:
            return "[PHONE_REDACTED]"

        t = _EMAIL_RX.sub(_email_sub, str(text))
        t = _PHONE_RX.sub(_phone_sub, t)
        self._log("pii_redact", {"length_in": len(text), "length_out": len(t)})
        return t


def federated_client_dp_update(
    update: GradMap,
    *,
    epsilon: float,
    gate: Any,
    max_norm: float,
    priv: Optional[SigmaPrivacy] = None,
) -> GradMap:
    """Hook for :mod:`cos.sigma_federated` — clip then σ-adaptive DP noise."""
    sp = priv or SigmaPrivacy()
    clipped = sp.gradient_clip(update, max_norm, gate)
    return sp.dp_noise(clipped, epsilon, gate)
