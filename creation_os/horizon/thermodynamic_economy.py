"""
Protocol 8 — Economic singularity: value from physics, not fiat.

V = Δσ / IPW  (intelligence per watt). Measures entropy removed vs energy spent.

1 = 1.
"""
from __future__ import annotations

from typing import Any, Dict


class ThermodynamicEconomy:
    def __init__(self) -> None:
        self.min_watts = 1e-9

    def value(self, delta_sigma: float, watts: float) -> float:
        w = max(self.min_watts, float(watts))
        return float(delta_sigma) / w

    def score_action(
        self,
        sigma_before: float,
        sigma_after: float,
        watts: float,
    ) -> Dict[str, Any]:
        delta = float(sigma_before) - float(sigma_after)
        v = self.value(delta, watts)
        return {
            "delta_sigma": delta,
            "watts": watts,
            "V": round(v, 8),
            "unit": "delta_sigma_per_watt",
            "invariant": "1=1",
        }
