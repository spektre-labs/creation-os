# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-cost — illustrative routing economics vs flat premium-model baselines.

Currency rates are **configurable placeholders**, not live invoices. See
``docs/CLAIM_DISCIPLINE.md`` — do not merge with harness benchmarks in headlines."""
from __future__ import annotations

from typing import Any, Callable, Dict, List, Optional

__all__ = ["SigmaCost"]


class SigmaCost:
    """Token-cost model + cascade / fleet hooks (lab)."""

    def __init__(
        self,
        gate: Any = None,
        *,
        eur_per_1k_tokens: Optional[Dict[str, float]] = None,
    ) -> None:
        from cos.sigma_gate import SigmaGate

        self.gate = gate or SigmaGate()
        self.eur_per_1k: Dict[str, float] = dict(
            eur_per_1k_tokens
            or {
                "gpt-4o": 0.005,
                "gpt-4o-mini": 0.00015,
                "local": 0.0,
                "sigma-cascade": 0.0008,
            },
        )
        self._daily_budget_eur: Optional[float] = None
        self._usage_log: List[Dict[str, Any]] = []
        self._disclaimer = (
            "Illustrative EUR from configurable tables; not live billing or guaranteed savings."
        )

    def cost_per_query(
        self,
        model: str,
        tokens: int,
        *,
        cascade_level: int = 1,
    ) -> Dict[str, Any]:
        rate = float(self.eur_per_1k.get(str(model), self.eur_per_1k.get("gpt-4o", 0.005)))
        mult = 1.0 + 0.04 * max(0, int(cascade_level) - 1)
        eur = max(0, int(tokens)) / 1000.0 * rate * mult
        return {
            "model": str(model),
            "tokens": int(tokens),
            "cascade_level": int(cascade_level),
            "eur": round(float(eur), 6),
            "disclaimer": self._disclaimer,
        }

    def cascade_savings(self, l1_sufficient_fraction: float = 0.7) -> Dict[str, Any]:
        """If ``l1_sufficient_fraction`` of queries stop at L1, estimate avoided upper tiers."""
        f = max(0.0, min(1.0, float(l1_sufficient_fraction)))
        avoided_upper = f
        return {
            "l1_fraction": f,
            "avoided_upper_tier_fraction": round(avoided_upper, 4),
            "note": "Toy decomposition; measured split requires route telemetry.",
            "disclaimer": self._disclaimer,
        }

    def fleet_routing_cost(
        self,
        fleet: Any,
        prompt: str,
        gate: Any,
        *,
        tokens_if_routed: int = 500,
    ) -> Dict[str, Any]:
        """Prefer cheapest registered model when σ is low (see ``SigmaFleet.route``)."""
        if fleet is None or not hasattr(fleet, "route"):
            return {"error": "no_fleet", "eur": 0.0}
        pick = fleet.route(str(prompt), gate)
        name = str(pick.get("name") or "local")
        return self.cost_per_query(name, tokens_if_routed, cascade_level=1)

    def budget(self, daily_eur: float) -> None:
        self._daily_budget_eur = float(daily_eur)

    def budget_sigma_scale(self, base_tau: float = 0.35) -> float:
        """Tighten accept threshold slightly when daily budget is finite (lab)."""
        if self._daily_budget_eur is None:
            return float(base_tau)
        # lean toward caution when a budget cap is set
        return min(0.45, float(base_tau) + 0.03)

    def record_usage(self, eur: float, *, label: str = "query") -> None:
        self._usage_log.append({"label": str(label), "eur": float(eur)})

    def report(self) -> Dict[str, Any]:
        total = sum(float(x["eur"]) for x in self._usage_log)
        by_label: Dict[str, float] = {}
        for row in self._usage_log:
            by_label[row["label"]] = by_label.get(row["label"], 0.0) + float(row["eur"])
        return {
            "entries": len(self._usage_log),
            "total_eur": round(total, 6),
            "by_label": {k: round(v, 6) for k, v in by_label.items()},
            "daily_budget_eur": self._daily_budget_eur,
            "disclaimer": self._disclaimer,
        }

    def projection(self, days: int, current_daily_eur: float) -> Dict[str, Any]:
        d = max(0, int(days))
        return {
            "days": d,
            "projected_eur": round(float(current_daily_eur) * d, 4),
            "disclaimer": self._disclaimer,
        }

    def comparison(
        self,
        *,
        queries: int,
        tokens_per_query: int,
        cascade_route_fn: Optional[Callable[[int], str]] = None,
    ) -> Dict[str, Any]:
        """``all-premium`` vs σ-shaped routing (placeholder split via ``cascade_route_fn``)."""
        n = max(1, int(queries))
        tok = max(1, int(tokens_per_query))

        def _route(i: int) -> str:
            if cascade_route_fn:
                return str(cascade_route_fn(i))
            return "sigma-cascade" if i % 3 != 0 else "gpt-4o"

        flat = self.cost_per_query("gpt-4o", tok * n, cascade_level=5)["eur"]
        mixed = sum(self.cost_per_query(_route(i), tok, cascade_level=1)["eur"] for i in range(n))
        saving = 0.0 if flat <= 0 else max(0.0, (flat - mixed) / flat)
        return {
            "queries": n,
            "all_premium_eur": round(flat, 6),
            "sigma_route_eur": round(mixed, 6),
            "fractional_savings_vs_flat": round(saving, 4),
            "disclaimer": self._disclaimer,
        }
