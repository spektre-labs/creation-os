# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
σ-JEPA lab scaffold: predict in a **latent** vector (no pixels), score prediction drift
with a σ-style scalar, and run the canonical **Q16** cognitive interrupt
(``sigma_gate_core`` — not ``sigma_gate.h``).
"""
from __future__ import annotations

import math
from typing import Any, Dict, List, Optional, Protocol, Sequence, Tuple

from cos.sigma_gate_core import SigmaState, Verdict, sigma_gate, sigma_update

K_RAW_DEFAULT = 0.92


def _clamp01(x: float) -> float:
    if x < 0.0:
        return 0.0
    if x > 1.0:
        return 1.0
    return float(x)


def _cosine_similarity_vecs(a: Sequence[float], b: Sequence[float]) -> float:
    if len(a) == 0 or len(b) == 0:
        return 0.0
    n = min(len(a), len(b))
    dot = 0.0
    na = 0.0
    nb = 0.0
    for i in range(n):
        x = float(a[i])
        y = float(b[i])
        dot += x * y
        na += x * x
        nb += y * y
    denom = math.sqrt(na) * math.sqrt(nb) + 1e-12
    return _clamp01(dot / denom)


class LatentEncoder(Protocol):
    def __call__(self, observation: Any) -> List[float]:
        ...

    def ema_copy(self) -> LatentEncoder:
        ...


class LatentPredictor(Protocol):
    def __call__(self, z: Sequence[float], action: Optional[str] = None) -> List[float]:
        ...

    def uncertainty(self, z_current: Sequence[float], z_next: Sequence[float]) -> float:
        ...


class LabLatentEncoder:
    """Deterministic UTF-8 → bounded ``dim``-vector encoder (lab only)."""

    def __init__(self, dim: int = 8, *, ema_blend: float = 1.0) -> None:
        self.dim = int(dim)
        self._ema_blend = float(ema_blend)

    def ema_copy(self) -> LabLatentEncoder:
        return LabLatentEncoder(self.dim, ema_blend=0.97 * self._ema_blend + 0.03)

    def __call__(self, observation: Any) -> List[float]:
        if isinstance(observation, (list, tuple)):
            raw = [float(x) for x in observation[: self.dim]]
        else:
            s = str(observation).encode("utf-8", errors="ignore")[: self.dim]
            raw = [float(b) / 255.0 for b in s]
        raw = raw + [0.0] * (self.dim - len(raw))
        return [_clamp01(v * self._ema_blend) for v in raw]


class LabLatentPredictor:
    """Small latent drift + optional action bump (lab only)."""

    def __init__(self, drift: float = 0.02) -> None:
        self.drift = float(drift)

    def __call__(self, z: Sequence[float], action: Optional[str] = None) -> List[float]:
        out = [_clamp01(float(z[i]) + self.drift + 0.005 * float(i % 5)) for i in range(len(z))]
        if action:
            bump = min(0.08, 0.004 * float(len(str(action))))
            out[0] = _clamp01(out[0] + bump)
        return out

    def uncertainty(self, z_current: Sequence[float], z_next: Sequence[float]) -> float:
        return 1.0 - _cosine_similarity_vecs(z_current, z_next)


class SigmaJEPA:
    """Encode → predict → measure (when a target latent is available)."""

    def __init__(
        self,
        encoder: LatentEncoder,
        predictor: LatentPredictor,
        gate: Optional[Any] = None,
        *,
        k_raw: float = K_RAW_DEFAULT,
    ) -> None:
        self.context_encoder = encoder
        self.target_encoder = encoder.ema_copy()
        self.predictor = predictor
        self.gate = gate
        self.k_raw = float(k_raw)

    @staticmethod
    def cosine_similarity(a: Sequence[float], b: Sequence[float]) -> float:
        return _cosine_similarity_vecs(a, b)

    def encode(self, observation: Any) -> List[float]:
        return list(self.context_encoder(observation))

    def predict(self, z_current: Sequence[float], action: Optional[str] = None) -> List[float]:
        return list(self.predictor(z_current, action))

    def measure_prediction_sigma(
        self, z_pred: Sequence[float], z_actual: Sequence[float]
    ) -> Tuple[float, Verdict]:
        similarity = _cosine_similarity_vecs(z_pred, z_actual)
        sigma = _clamp01(1.0 - similarity)
        st = SigmaState()
        sigma_update(st, sigma, self.k_raw)
        verdict = sigma_gate(st)
        if self.gate is not None and hasattr(self.gate, "after_verdict"):
            getattr(self.gate, "after_verdict")(sigma, verdict)
        return sigma, verdict

    def world_model_step(self, observation: Any, action: Optional[str] = None) -> Dict[str, Any]:
        z_current = self.encode(observation)
        z_pred = self.predict(z_current, action)
        return {
            "z_current": z_current,
            "z_predicted": z_pred,
            "ready_to_measure": True,
        }

    def update_on_observation(
        self, prediction: Dict[str, Any], next_observation: Any
    ) -> Dict[str, Any]:
        z_actual = list(self.target_encoder(next_observation))
        z_pred = prediction["z_predicted"]
        sigma, verdict = self.measure_prediction_sigma(z_pred, z_actual)
        return {
            "sigma_model": sigma,
            "verdict": verdict,
            "prediction_error": sigma,
            "world_model_quality": 1.0 - sigma,
        }


__all__ = [
    "K_RAW_DEFAULT",
    "LabLatentEncoder",
    "LabLatentPredictor",
    "LatentEncoder",
    "LatentPredictor",
    "SigmaJEPA",
]
