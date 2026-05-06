# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Heuristic JEPA-style world model with σ-gate verdicts (latent-space prediction error).

This module is a **portable lab stub**: predictions are fixed-dimension latent vectors
(heuristic encoder; no ViT/CNN). **σ** is normalized cosine mismatch between predicted and
realized latents—internal “surprise” in representation space, complementary to
:class:`~cos.sigma_gate.SigmaGate` on text. It does **not** ship a trained V-JEPA-class
video encoder; optional ``numpy`` speeds vector math. See ``docs/CLAIM_DISCIPLINE.md``.
"""
from __future__ import annotations

import math
from typing import Any, Dict, List, Optional, Sequence, Union

from cos.config import SigmaConfig
from cos.sigma_gate import SigmaGate

try:
    import numpy as np

    _HAS_NP = True
except ImportError:
    np = None  # type: ignore[assignment]
    _HAS_NP = False

__all__ = ["SigmaJEPA"]

Observation = Union[str, Sequence[float], Sequence[int]]


class SigmaJEPA:
    """σ-validated world model: encode → predict next latent → compare → gate verdict."""

    def __init__(self, gate: Any = None, *, dim: int = 256) -> None:
        self.gate = gate or SigmaGate()
        self.dim = int(dim)
        self.states: List[Any] = []
        self.predictions: List[Any] = []
        self.σ_history: List[Dict[str, Any]] = []

    def _sigma_cfg(self) -> SigmaConfig:
        return SigmaConfig(
            threshold_accept=float(getattr(self.gate, "threshold_accept", 0.15)),
            threshold_abstain=float(getattr(self.gate, "threshold_abstain", 0.85)),
        )

    def encode(self, observation: Observation) -> Any:
        """Encode an observation into a ``dim``-vector (heuristic; no trained encoder)."""
        if _HAS_NP:
            if isinstance(observation, str):
                h = hash(observation) % (2**31)
                rng = np.random.RandomState(int(h))
                return rng.standard_normal(self.dim).astype(np.float32)
            arr = np.asarray(observation, dtype=np.float32).ravel()
            if arr.size < self.dim:
                arr = np.pad(arr, (0, self.dim - int(arr.size)))
            return arr[: self.dim].astype(np.float32, copy=False)
        # Pure Python fallback: deterministic pseudo-random unit-scale vector
        seed = hash(str(observation)) % (2**31)
        x = float(seed)
        out: List[float] = []
        for i in range(self.dim):
            x = (x * 1103515245 + 12345 + i) % (2**31)
            out.append((x % 10000) / 10000.0 - 0.5)
        return out

    def predict(self, prior_state: Any, action: Optional[str] = None) -> Any:
        """Predict next latent from ``prior_state``; optional ``action`` reserved for lab."""
        _ = action
        if _HAS_NP:
            state = np.asarray(prior_state, dtype=np.float32).ravel()
            if state.size != self.dim:
                state = np.pad(state, (0, max(0, self.dim - int(state.size))))[: self.dim]
            if len(self.states) >= 2:
                prev = np.asarray(self.states[-2], dtype=np.float32).ravel()[: self.dim]
                curr = np.asarray(self.states[-1], dtype=np.float32).ravel()[: self.dim]
                velocity = curr - prev
                predicted = state.astype(np.float64) + velocity.astype(np.float64) * 0.5
                return predicted.astype(np.float32).tolist()
            noise = np.random.randn(self.dim).astype(np.float32) * 0.01
            return (state + noise).tolist()
        # list fallback
        st = [float(x) for x in list(prior_state)[: self.dim]]
        while len(st) < self.dim:
            st.append(0.0)
        if len(self.states) >= 2:
            prev = [float(x) for x in list(self.states[-2])[: self.dim]]
            curr = [float(x) for x in list(self.states[-1])[: self.dim]]
            velocity = [c - p for p, c in zip(prev, curr)]
            return [st[i] + 0.5 * velocity[i] for i in range(self.dim)]
        incr = [math.sin(float(i + hash(tuple(st)) % 997)) * 0.01 for i in range(self.dim)]
        return [st[i] + incr[i] for i in range(self.dim)]

    def σ_prediction(self, predicted_state: Any, actual_state: Any) -> float:
        """σ in ``[0,1]`` from cosine distance: ``(1 - cos_sim) / 2``."""
        if _HAS_NP:
            pred = np.asarray(predicted_state, dtype=np.float64).ravel()
            actual = np.asarray(actual_state, dtype=np.float64).ravel()
            d = min(int(pred.size), int(actual.size), self.dim)
            pred = pred[:d]
            actual = actual[:d]
            dot = float(np.dot(pred, actual))
            norm_p = float(np.linalg.norm(pred)) + 1e-8
            norm_a = float(np.linalg.norm(actual)) + 1e-8
            cosine_sim = dot / (norm_p * norm_a)
            cosine_sim = max(-1.0, min(1.0, cosine_sim))
            σ = max(0.0, min(1.0, (1.0 - cosine_sim) / 2.0))
            return round(σ, 4)
        p = [float(x) for x in list(predicted_state)[: self.dim]]
        a = [float(x) for x in list(actual_state)[: self.dim]]
        while len(p) < self.dim:
            p.append(0.0)
        while len(a) < self.dim:
            a.append(0.0)
        dot = sum(x * y for x, y in zip(p, a))
        norm_p = math.sqrt(sum(x * x for x in p)) + 1e-8
        norm_a = math.sqrt(sum(y * y for y in a)) + 1e-8
        cosine_sim = max(-1.0, min(1.0, dot / (norm_p * norm_a)))
        σ = max(0.0, min(1.0, (1.0 - cosine_sim) / 2.0))
        return round(σ, 4)

    def step(self, observation: Observation, action: Optional[str] = None) -> Dict[str, Any]:
        """Observe → encode → (optional) predict vs previous → σ_latent → ΣConfig verdict."""
        current_state = self.encode(observation)
        σ = 0.0
        if self.states:
            predicted = self.predict(self.states[-1], action)
            self.predictions.append(predicted)
            σ = self.σ_prediction(predicted, current_state)

        self.states.append(current_state)
        cfg = self._sigma_cfg()
        verdict = cfg.verdict(float(σ))
        surprise = float(σ) > 0.5
        self.σ_history.append({"σ": float(σ), "verdict": verdict})

        if _HAS_NP and isinstance(current_state, np.ndarray):
            out_vec = current_state.tolist()
        else:
            out_vec = list(current_state)  # type: ignore[arg-type]
        return {
            "σ": float(σ),
            "verdict": verdict,
            "surprise": surprise,
            "state_dim": len(out_vec),
        }

    def surprise_rate(self) -> float:
        if not self.σ_history:
            return 0.0
        surprises = sum(1 for h in self.σ_history if float(h["σ"]) > 0.5)
        return surprises / len(self.σ_history)

    def avg_σ(self) -> float:
        if not self.σ_history:
            return 0.0
        return sum(float(h["σ"]) for h in self.σ_history) / len(self.σ_history)
