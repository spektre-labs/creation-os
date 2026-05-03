# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-budget — compute/EUR budgets conditioned on σ (lab tables, not billing truth).

Verdict costs are illustrative. See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

from typing import Any, Dict, Mapping, Optional, Union

__all__ = ["SigmaBudget"]

_Query = Union[str, Mapping[str, Any]]


class SigmaBudget:
    """Route compute depth from σ; track daily caps and toy ROI."""

    def __init__(self, gate: Any = None) -> None:
        from cos.sigma_gate import SigmaGate

        self.gate = gate or SigmaGate()
        self._spent_day_eur = 0.0
        self._cascade_eur = {"L1": 0.001, "L2": 0.01, "L3": 0.05, "L4": 0.05, "L5": 0.05}
        self._verdict_cost = {"ACCEPT": 0.0005, "RETHINK": 0.003, "ABSTAIN": 0.0001}

    def allocate(self, query: _Query, gate: Optional[Any] = None) -> Dict[str, Any]:
        """Derive cascade depth + EUR estimate from σ (prompt/response in ``query``)."""
        g = gate or self.gate
        if isinstance(query, str):
            prompt, response = str(query), ""
        else:
            prompt = str(query.get("prompt", ""))
            response = str(query.get("response", ""))
        sigma, verdict = g.score(prompt, response)
        s = float(sigma)
        if s < 0.25:
            tier, budget_eur, levels = "lite", 0.002, ("L1",)
        elif s < 0.55:
            tier, budget_eur, levels = "mid", 0.02, ("L1", "L2", "L3")
        else:
            tier, budget_eur, levels = "heavy", 0.12, ("L1", "L2", "L3", "L4", "L5")
        return {
            "tier": tier,
            "sigma": round(s, 6),
            "verdict": str(verdict),
            "budget_eur": round(float(budget_eur), 6),
            "cascade_levels": levels,
        }

    @staticmethod
    def token_budget(remaining_eur: float, avg_cost_per_token: float) -> Dict[str, Any]:
        """How many tokens remain at an average EUR/token."""

        rate = float(max(avg_cost_per_token, 1e-12))
        n = int(max(0.0, float(remaining_eur)) / rate)
        return {"tokens_remaining": n, "avg_cost_per_token": rate}

    def cascade_budget(self) -> Dict[str, float]:
        """Static per-level EUR placeholders (extend in fleet wiring)."""
        return dict(self._cascade_eur)

    def daily_cap(self, eur: float, *, spend_increment: Optional[float] = None) -> Dict[str, Any]:
        """Return whether daily spend is exhausted; optionally add ``spend_increment``."""
        cap = float(max(0.0, eur))
        if spend_increment is not None:
            self._spent_day_eur += float(max(0.0, spend_increment))
        left = max(0.0, cap - self._spent_day_eur)
        return {
            "cap_eur": cap,
            "spent_eur": round(self._spent_day_eur, 6),
            "remaining_eur": round(left, 6),
            "halt": self._spent_day_eur >= cap and cap > 0,
        }

    def cost_per_verdict(self, verdict: str) -> float:
        """Illustrative EUR per gate outcome."""
        return float(self._verdict_cost.get(str(verdict).upper(), 0.001))

    @staticmethod
    def sigma_roi(value_eur: float, cost_eur: float) -> Dict[str, Any]:
        """Value divided by σ-gate spend (toy KPI)."""

        c = float(max(cost_eur, 1e-9))
        v = float(value_eur)
        return {"roi": round(v / c, 6), "value_eur": v, "cost_eur": c}
