# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""**Living weights** lab toy: a global σ readout drives per-weight EMAs and crude reset / nudge rules.

Plasticity loss in deep nets and inference-time adaptation are **active research** topics; this
module is **not** a Nature 2024 replication, **not** biologically faithful Hebbian learning, and
**not** proof that σ solves plasticity as a theorem. The **kernel** story points at portable
``sigma_gate.h``: **do not mutate** that header from here. **Firmware** in this file means these
scalar buffers only. **NumPy is optional** (list fallback). **NOT AGI achieved.** See
``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import random
from typing import Any, Dict, List

from cos.sigma_gate import SigmaGate

__all__ = ["LivingWeights"]

try:
    import numpy as np

    _HAS_NP = True
except ImportError:
    np = None  # type: ignore[assignment, misc]
    _HAS_NP = False


def _sigma_list(lw: "LivingWeights") -> List[float]:
    if _HAS_NP:
        return [float(x) for x in lw.σ_per_weight.tolist()]  # type: ignore[attr-defined]
    return [float(x) for x in lw.σ_per_weight]


class LivingWeights:
    """Scalar weight vector with σ-gated reset / consolidate heuristics."""

    def __init__(
        self,
        gate: Any = None,
        n_weights: int = 100,
        *,
        reset_threshold: float = 0.8,
        strengthen_threshold: float = 0.2,
    ) -> None:
        self.gate = gate or SigmaGate()
        self.n_weights = max(1, int(n_weights))
        self.reset_threshold = float(reset_threshold)
        self.strengthen_threshold = float(strengthen_threshold)

        if _HAS_NP:
            rng = np.random.default_rng(42)
            self.weights = rng.normal(0.0, 0.01, size=self.n_weights)
            self.σ_per_weight = np.full(self.n_weights, 0.5)  # type: ignore[union-attr]
            self.usage_count = np.zeros(self.n_weights, dtype=np.float64)  # type: ignore[union-attr]
            self.age = np.zeros(self.n_weights, dtype=np.float64)  # type: ignore[union-attr]
        else:
            random.seed(42)
            self.weights = [random.gauss(0, 0.01) for _ in range(self.n_weights)]
            self.σ_per_weight = [0.5] * self.n_weights
            self.usage_count = [0.0] * self.n_weights
            self.age = [0.0] * self.n_weights

        self.generation = 0
        self.resets = 0
        self.strengthens = 0

    def step(self, input_data: Any) -> Dict[str, Any]:
        """Forward dot (toy input), score with gate, EMA σ per weight, then plasticity."""
        self.generation += 1
        scalar_in = (hash(str(input_data)) % 256) / 256.0

        if _HAS_NP:
            inp = np.full(self.n_weights, scalar_in, dtype=np.float64)
            output = float(np.dot(self.weights, inp))
        else:
            inp = [scalar_in] * self.n_weights
            output = float(sum(w * i for w, i in zip(self.weights, inp)))

        σ, verdict = self.gate.score(str(input_data), str(output))
        sigma = float(σ)

        for i in range(self.n_weights):
            if _HAS_NP:
                self.σ_per_weight[i] = 0.9 * self.σ_per_weight[i] + 0.1 * sigma  # type: ignore[index]
                self.age[i] += 1.0
            else:
                self.σ_per_weight[i] = 0.9 * self.σ_per_weight[i] + 0.1 * sigma
                self.age[i] += 1.0

        actions = self._apply_plasticity()

        return {
            "output": output,
            "σ": round(sigma, 4),
            "sigma": round(sigma, 4),
            "verdict": verdict,
            "generation": self.generation,
            "actions": actions,
        }

    def _apply_plasticity(self) -> Dict[str, int]:
        resets = 0
        strengthens = 0
        rt = self.reset_threshold
        st = self.strengthen_threshold

        for i in range(self.n_weights):
            σ_w = float(self.σ_per_weight[i]) if not _HAS_NP else float(self.σ_per_weight[i])  # type: ignore[index]

            if σ_w > rt:
                if _HAS_NP:
                    rng_r = np.random.default_rng(42 + self.resets + i + self.generation * 31)
                    self.weights[i] = float(rng_r.normal(0.0, 0.01))
                    self.σ_per_weight[i] = 0.5
                    self.age[i] = 0.0
                else:
                    self.weights[i] = random.gauss(0, 0.01)
                    self.σ_per_weight[i] = 0.5
                    self.age[i] = 0.0
                resets += 1
            elif σ_w < st:
                self.weights[i] *= 1.01
                strengthens += 1

        self.resets += resets
        self.strengthens += strengthens
        return {"resets": resets, "strengthens": strengthens}

    def hebbian_update(self, pre: Any, post: float, *, learning_rate: float = 0.001) -> None:
        """σ-gated outer-product style nudge; **NumPy path only** (length `n_weights` ``pre``)."""
        if not _HAS_NP:
            return
        pre_a = np.asarray(pre, dtype=np.float64).ravel()
        post_f = float(post)
        lr = float(learning_rate)
        st_z, en_z = 0.2, 0.7
        for i in range(min(len(pre_a), self.n_weights)):
            σ_w = float(self.σ_per_weight[i])
            if st_z < σ_w < en_z:
                delta = lr * float(pre_a[i]) * post_f
                self.weights[i] += delta  # type: ignore[index]
                self.usage_count[i] += 1.0  # type: ignore[index]

    def plasticity_report(self) -> Dict[str, Any]:
        sigs = _sigma_list(self)
        avg_σ = sum(sigs) / len(sigs)
        ages = [float(self.age[i]) for i in range(self.n_weights)]
        avg_age = sum(ages) / len(ages)
        dead = sum(1 for s in sigs if s > self.reset_threshold)
        stable = sum(1 for s in sigs if s < self.strengthen_threshold)
        plasticity = round(1.0 - dead / self.n_weights, 4)

        if dead < self.n_weights * 0.1:
            interp = "highly plastic — learning actively"
        elif dead > self.n_weights * 0.5:
            interp = "losing plasticity — many high-σ slots"
        else:
            interp = "moderate plasticity"

        return {
            "generation": self.generation,
            "avg_σ": round(avg_σ, 4),
            "avg_sigma": round(avg_σ, 4),
            "avg_age": round(avg_age, 1),
            "dead_weights": dead,
            "stable_weights": stable,
            "learning_weights": self.n_weights - dead - stable,
            "total_resets": self.resets,
            "total_strengthens": self.strengthens,
            "plasticity": plasticity,
            "interpretation": interp,
        }

    def kernel_vs_firmware(self) -> Dict[str, Any]:
        """Pedagogical split: stable σ kernel narrative vs these mutable scalars."""
        sigs = _sigma_list(self)
        dead = sum(1 for s in sigs if s > self.reset_threshold)
        p = round(1.0 - dead / self.n_weights, 4)
        return {
            "kernel": {
                "type": "sigma_gate.h (portable header)",
                "changes": "out-of-band in this Python module",
                "note": "Do not edit sigma_gate.h from living_weights.py.",
                "language": "C89 narrative",
            },
            "firmware": {
                "type": "living_weights",
                "changes": "per step() optional reset/nudge",
                "size": f"{self.n_weights} scalars",
                "resets": self.resets,
                "strengthens": self.strengthens,
                "plasticity": p,
            },
            "principle": "Stable scoring kernel + mutable lab weights (not AGI).",
        }
