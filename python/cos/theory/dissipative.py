# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Dissipative-structure **metaphor**: σ traces as a toy “distance from equilibrium” readout.

Boltzmann-style **large-fluctuation suppression** is illustrated with an exponential of a
scaled |Δσ| (not a literal partition function). Prigogine’s **far-from-equilibrium**
maintained structures motivate the *narrative* that ongoing σ-minimization corresponds to
**order through turnover** in the lab trace — **not** a measured entropy-production tensor,
**not** thermodynamic proof of life or consciousness, and **not** AGI achieved.

External discussions sometimes link **FDT violation** to **non-equilibrium** biomarkers; this
module **does not** implement fluctuation–dissipation diagnostics or neuroscience claims.
See ``docs/CLAIM_DISCIPLINE.md``.
"""
from __future__ import annotations

import math
from typing import Any, Dict, List

from cos.sigma_gate import SigmaGate

__all__ = ["DissipativeStructure"]


def _verdict_str(verdict: Any) -> str:
    return str(getattr(verdict, "name", verdict))


class DissipativeStructure:
    """Toy σ-trace harness inspired by Boltzmann / Prigogine *pedagogy* only."""

    def __init__(self, gate: Any = None) -> None:
        self.gate = gate or SigmaGate()
        self.sigma_trace: List[float] = []
        self.entropy_production: List[float] = []
        self.alive = True

    def measure_distance_from_equilibrium(self) -> float:
        """Lab scalar: ``1 − mean(recent σ)`` — high when average mismatch is *low*."""
        if not self.sigma_trace:
            return 0.5
        n = min(len(self.sigma_trace), 10)
        avg_s = sum(self.sigma_trace[-n:]) / n
        distance = 1.0 - float(avg_s)
        return round(max(0.0, min(1.0, distance)), 4)

    def fluctuation_probability(self, delta_sigma: float) -> float:
        """Scaled toy: ``P ∝ exp(−10 · |Δσ|)`` — large excursions suppressed, not physical kT."""
        return round(math.exp(-abs(float(delta_sigma)) * 10.0), 6)

    def entropy_rate(self, window: int = 5) -> float:
        """Mean |Δσ| over the last ``window`` samples; appended to ``entropy_production``."""
        if len(self.sigma_trace) < 2:
            return 0.0
        recent = self.sigma_trace[-max(2, int(window)) :]
        diffs = [abs(recent[i + 1] - recent[i]) for i in range(len(recent) - 1)]
        rate = sum(diffs) / len(diffs) if diffs else 0.0
        self.entropy_production.append(float(rate))
        return round(float(rate), 6)

    def is_dissipative(self) -> Dict[str, Any]:
        """Heuristic dissipative *story*: ordered + active turnover + non-increasing σ tail."""
        if len(self.sigma_trace) < 5:
            return {"dissipative": False, "reason": "insufficient data"}

        distance = self.measure_distance_from_equilibrium()
        rate = self.entropy_rate()
        trend = float(self.sigma_trace[-1]) - float(self.sigma_trace[-5])

        is_ordered = distance > 0.3
        is_active = rate > 0.001
        not_dissolving = trend <= 0.0

        if is_ordered and is_active and not_dissolving:
            interp = "alive: ordered, active, stable"
        elif not is_ordered:
            interp = "dying: dissolving toward equilibrium"
        else:
            interp = "critical: order maintained but σ rising"

        return {
            "dissipative": bool(is_ordered and is_active and not_dissolving),
            "distance_from_equilibrium": distance,
            "entropy_rate": rate,
            "σ_trend": round(trend, 4),
            "sigma_trend": round(trend, 4),
            "interpretation": interp,
        }

    def step(self, observation: Any) -> Dict[str, Any]:
        """Score ``observation``; update trace; refresh dissipative diagnostic; maybe clear ``alive``."""
        sigma, verdict = self.gate.score("dissipative step", str(observation))
        s = float(sigma)
        self.sigma_trace.append(s)

        state = self.is_dissipative()
        if not state.get("dissipative", False) and len(self.sigma_trace) > 10:
            self.alive = False

        return {
            "σ": round(s, 4),
            "sigma": round(s, 4),
            "verdict": _verdict_str(verdict),
            "alive": self.alive,
            "state": state,
        }
