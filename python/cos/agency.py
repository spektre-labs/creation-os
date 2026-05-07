# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Agency lab: **σ-weighted stochastic choice** (Boltzmann), counterfactual regret, autonomy read.

Not a claim of strong autonomy — see ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import math
import random
from typing import Any, Dict, List, Sequence

from cos.sigma_gate import SigmaGate

__all__ = ["SigmaAgency"]


def _verdict_str(verdict: Any) -> str:
    raw = str(getattr(verdict, "name", verdict))
    return raw.split(".")[-1] if "." in raw else raw


class SigmaAgency:
    """σ-landscape navigation: **P(i) ∝ exp(−σ_i / T)** — neither pure argmin nor uniform random."""

    def __init__(self, gate: Any = None) -> None:
        self.gate = gate or SigmaGate()
        self.decision_history: List[Dict[str, Any]] = []

    def choose(
        self,
        options: Sequence[str],
        context: str = "",
        temperature: float = 1.0,
    ) -> Dict[str, Any]:
        """Boltzmann sampling over gate σ; lower σ remains **more likely** at fixed T."""
        ctx = str(context).strip()
        scored: List[Dict[str, Any]] = []
        for option in options:
            opt = str(option)
            σ, verdict = self.gate.score(ctx, opt)
            scored.append(
                {
                    "option": opt,
                    "σ": float(σ),
                    "verdict": _verdict_str(verdict),
                },
            )
        T = max(float(temperature), 0.01)
        weights = [math.exp(-float(s["σ"]) / T) for s in scored]
        total = sum(weights)
        probs = [w / total for w in weights]
        chosen_idx = random.choices(range(len(scored)), weights=probs, k=1)[0]
        chosen = scored[chosen_idx]

        result = {
            "chosen": chosen["option"],
            "σ": round(chosen["σ"], 4),
            "verdict": chosen["verdict"],
            "probability": round(float(probs[chosen_idx]), 4),
            "alternatives": [
                {
                    "option": scored[i]["option"],
                    "σ": round(scored[i]["σ"], 4),
                    "probability": round(float(probs[i]), 4),
                }
                for i in range(len(scored))
                if i != chosen_idx
            ],
            "temperature": float(temperature),
        }
        self.decision_history.append(result)
        return result

    def counterfactual(self, decision: str, alternative: str) -> Dict[str, Any]:
        """Compare σ of taken vs counterfactual choice on a shared framing."""
        frame = "decision_under_review"
        σ_chosen, _ = self.gate.score(frame, str(decision))
        σ_alt, _ = self.gate.score(frame, str(alternative))
        sc, sa = float(σ_chosen), float(σ_alt)
        return {
            "chosen": str(decision),
            "alternative": str(alternative),
            "σ_chosen": round(sc, 4),
            "σ_alternative": round(sa, 4),
            "regret": round(max(0.0, sc - sa), 4),
            "relief": round(max(0.0, sa - sc), 4),
        }

    def autonomy_score(self) -> float:
        """Proxy: **1 − mean(σ)** over recorded choices (lower historical stress ⇒ higher readout)."""
        if not self.decision_history:
            return 0.0
        avg_σ = sum(float(d["σ"]) for d in self.decision_history) / float(len(self.decision_history))
        return round(max(0.0, min(1.0, 1.0 - avg_σ)), 4)
