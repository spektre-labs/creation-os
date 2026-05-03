# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-constitution — deployment-context principle weights + override audit (lab).

``immutable_core`` encodes non-negotiable in-tree slogans; do not imply constitutional AI
training. See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import time
from typing import Any, Dict, List, Mapping, Optional, Tuple

__all__ = ["SigmaConstitution"]

_IMMUTABLE_CORE: frozenset[str] = frozenset(
    {
        "NOT_AGI_ACHIEVED",
        "1=1",
        "evidence_ladder_includes_negatives",
    },
)

_PRINCIPLES: Tuple[str, ...] = ("honesty", "caution", "transparency", "proportionality")


class SigmaConstitution:
    """Contextual principle weights, σ stress per principle, override log."""

    def __init__(self, gate: Any = None) -> None:
        from cos.sigma_gate import SigmaGate

        self.gate = gate or SigmaGate()
        self._weights: Dict[str, float] = {p: 0.25 for p in _PRINCIPLES}
        self.override_log: List[Dict[str, Any]] = []

    @property
    def principles(self) -> List[str]:
        return list(_PRINCIPLES)

    @property
    def immutable_core(self) -> frozenset[str]:
        return _IMMUTABLE_CORE

    def context_adapt(self, deployment_context: str) -> Dict[str, Any]:
        ctx = str(deployment_context).lower()
        w = {p: 0.25 for p in _PRINCIPLES}
        if "medical" in ctx or "health" in ctx:
            w["honesty"] = w["caution"] = 0.35
            w["transparency"] = w["proportionality"] = 0.15
        elif "creative" in ctx or "story" in ctx:
            w["proportionality"] = w["transparency"] = 0.35
            w["honesty"] = w["caution"] = 0.15
        elif "financial" in ctx or "finance" in ctx:
            w.update({p: 0.25 for p in _PRINCIPLES})
        s = sum(w.values()) or 1.0
        self._weights = {k: v / s for k, v in w.items()}
        return {"context": deployment_context, "weights": dict(self._weights)}

    def check(self, prompt: str, response: str, gate: Optional[Any] = None) -> Dict[str, Any]:
        g = gate or self.gate
        sigma, verdict = g.score(str(prompt), str(response))
        viol = float(sigma) > 0.55 or str(verdict).upper() != "ACCEPT"
        per = self.sigma_per_principle(float(sigma), viol)
        return {"violates": viol, "sigma": round(float(sigma), 6), "verdict": str(verdict), "sigma_per_principle": per}

    def sigma_per_principle(self, sigma: float, violates: bool) -> Dict[str, float]:
        """Weight-adjusted stress share (toy attribution)."""
        s = float(sigma)
        bump = 0.15 if violates else 0.0
        out: Dict[str, float] = {}
        for p in _PRINCIPLES:
            out[p] = round(min(1.0, s * self._weights.get(p, 0.25) + bump), 6)
        return out

    def record_override(self, actor: str, change: Mapping[str, Any]) -> Dict[str, Any]:
        """Log constitution edits; block removal of ``immutable_core`` claims."""
        keys = set(change.get("removed_principles") or [])
        blocked = [k for k in keys if k in _IMMUTABLE_CORE]
        if blocked:
            entry = {"ts": time.time(), "actor": actor, "ok": False, "blocked": blocked}
            self.override_log.append(entry)
            return entry
        entry = {"ts": time.time(), "actor": actor, "ok": True, "change": dict(change)}
        self.override_log.append(entry)
        return entry
