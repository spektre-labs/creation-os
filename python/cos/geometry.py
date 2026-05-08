# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""**Information geometry** sketch by identifying scalar σ∈(0,1) with a **Bernoulli** error rate.

The **Fisher–Rao** length element and **Kullback–Leibler** compares on that **1D** model are
standard. This file is **not** the geometry of full Creation-OS **LM** parameter space, **not**
a certified natural-gradient optimizer, and **not** a uniqueness claim for global “recovery’’
paths — only **closed-form toys** on the interval with :class:`~cos.sigma_gate.SigmaGate` kept as
an optional **alignment** handle (scores are unused in the pure formulas). **Zero** third-party
dependencies. **Not AGI achieved.** See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import math
from typing import Any, Dict, List, Sequence

from cos.sigma_gate import SigmaGate

__all__ = ["SigmaManifold"]


def _clip_prob(x: float, *, eps: float = 1e-3) -> float:
    return max(float(eps), min(1.0 - float(eps), float(x)))


class SigmaManifold:
    """Toy **σ-manifold** tools: Fisher–Rao distance, KL, natural-gradient step, geodesic sampler."""

    def __init__(self, gate: Any = None) -> None:
        self.gate = gate or SigmaGate()

    def fisher_distance(self, sigma_a: float, sigma_b: float) -> float:
        """Fisher–Rao distance for Bernoulli(σ) parameters (Bhattacharyya / Hellinger related form)."""
        a = _clip_prob(sigma_a)
        b = _clip_prob(sigma_b)
        inner = math.sqrt(a * b) + math.sqrt((1.0 - a) * (1.0 - b))
        inner = max(-1.0, min(1.0, inner))
        return round(2.0 * math.acos(inner), 4)

    def kl_divergence(self, sigma_a: float, sigma_b: float) -> float:
        """KL( σ_a ‖ σ_b ) under Bernoulli identification (asymmetric)."""
        a = _clip_prob(sigma_a)
        b = _clip_prob(sigma_b)
        kl = a * math.log(a / b) + (1.0 - a) * math.log((1.0 - a) / (1.0 - b))
        return round(max(0.0, float(kl)), 6)

    def natural_gradient(
        self,
        sigma_current: float,
        sigma_target: float = 0.0,
        learning_rate: float = 0.1,
    ) -> Dict[str, Any]:
        """One step: ``natural ∝ (d/dσ loss) / I_F(σ)`` with identity metric scaled by Fisher."""
        σ = _clip_prob(sigma_current)
        fisher_info = 1.0 / (σ * (1.0 - σ))
        euclidean_grad = float(sigma_current) - float(sigma_target)
        natural_grad = euclidean_grad / fisher_info
        lr = float(learning_rate)
        new_σ = float(sigma_current) - lr * natural_grad
        new_σ = max(0.0, min(1.0, new_σ))

        return {
            "σ_before": round(float(sigma_current), 4),
            "σ_after": round(new_σ, 4),
            "sigma_before": round(float(sigma_current), 4),
            "sigma_after": round(new_σ, 4),
            "euclidean_step": round(lr * euclidean_grad, 6),
            "natural_step": round(lr * natural_grad, 6),
            "fisher_info": round(fisher_info, 4),
            "speedup": round(
                abs(euclidean_grad) / max(abs(natural_grad), 1e-10),
                2,
            ),
        }

    def geodesic(self, sigma_start: float, sigma_end: float, n_steps: int = 10) -> Dict[str, Any]:
        """SLERP-style lift in ``θ = arcsin(√σ)``, matching the user sketch (not a full proof of minimality)."""
        path: List[float] = []
        n = max(1, int(n_steps))
        s0 = _clip_prob(sigma_start)
        s1 = _clip_prob(sigma_end)
        θ0 = math.asin(math.sqrt(s0))
        θ1 = math.asin(math.sqrt(s1))
        for i in range(n + 1):
            t = i / n
            θ_t = θ0 + t * (θ1 - θ0)
            σ_t = math.sin(θ_t) ** 2
            path.append(round(float(σ_t), 4))

        return {
            "path": path,
            "length": self.fisher_distance(sigma_start, sigma_end),
            "start": round(float(sigma_start), 4),
            "end": round(float(sigma_end), 4),
            "is_recovery": float(sigma_end) < float(sigma_start),
        }

    def curvature_at(self, sigma: float) -> Dict[str, Any]:
        """Proxy scalar curvature ``κ = 1/(4σ(1-σ))`` for the Bernoulli Fisher line element story."""
        σ = _clip_prob(sigma)
        curvature = 1.0 / (4.0 * σ * (1.0 - σ))
        if curvature > 5.0:
            interpret = "high curvature — near boundary, small changes matter a lot"
        elif curvature > 2.0:
            interpret = "moderate curvature"
        else:
            interpret = "low curvature — flat region, changes are gradual"

        return {
            "σ": round(σ, 4),
            "sigma": round(σ, 4),
            "curvature": round(curvature, 4),
            "interpretation": interpret,
        }

    def sigma_gradient_flow(
        self,
        sigma_trace: Sequence[float],
        target: float = 0.0,
    ) -> Dict[str, Any]:
        """Compare cumulative Fisher length vs endpoint distance (triangle inequality proxy)."""
        _ = target
        tr = [float(x) for x in sigma_trace]
        if len(tr) < 2:
            return {
                "efficient": True,
                "path_length_fisher": 0.0,
                "path_length_euclidean": 0.0,
                "direct_distance": 0.0,
                "geodesic_efficiency": 1.0,
                "interpretation": "insufficient samples",
            }

        total_fisher = 0.0
        total_euclidean = 0.0
        for i in range(len(tr) - 1):
            fd = float(self.fisher_distance(tr[i], tr[i + 1]))
            total_fisher += fd
            total_euclidean += abs(tr[i] - tr[i + 1])

        direct = float(self.fisher_distance(tr[0], tr[-1]))
        efficiency = direct / max(total_fisher, 1e-4)

        if efficiency > 0.7:
            interpret = "near-geodesic — efficient recovery"
        elif efficiency < 0.3:
            interpret = "wandering — inefficient path"
        else:
            interpret = "moderate efficiency"

        return {
            "path_length_fisher": round(total_fisher, 4),
            "path_length_euclidean": round(total_euclidean, 4),
            "direct_distance": round(direct, 4),
            "geodesic_efficiency": round(efficiency, 4),
            "efficient": efficiency > 0.7,
            "interpretation": interpret,
        }

    def σ_gradient_flow(  # noqa: PLC2401 — σ-spelled API
        self,
        sigma_trace: Sequence[float],
        target: float = 0.0,
    ) -> Dict[str, Any]:
        return self.sigma_gradient_flow(sigma_trace, target)
