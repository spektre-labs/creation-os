# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-cost — illustrative routing economics vs flat premium-model baselines.

:class:`CostManager` adds **USD-shaped** placeholder tariffs and σ-driven model picking (lab).
:class:`SigmaCost` keeps EUR-style bench hooks. Rates are **not** live invoices. See
``docs/CLAIM_DISCIPLINE.md`` — do not merge placeholder savings with harness rows in headlines."""
from __future__ import annotations

import time
from typing import Any, Callable, Dict, List, Optional, Tuple

from cos.sigma_gate import SigmaGate

__all__ = ["MODEL_COSTS", "CostManager", "SigmaCost"]


MODEL_COSTS: Dict[str, Tuple[float, float]] = {
    # model_name: (input_cost_per_1M, output_cost_per_1M) USD placeholders
    "local_7b": (0.0, 0.0),
    "local_3b": (0.0, 0.0),
    "qwen3_a3b": (0.0, 0.0),
    "gpt4o_mini": (0.15, 0.60),
    "claude_haiku": (0.25, 1.25),
    "gpt4o": (2.50, 10.00),
    "claude_sonnet": (3.00, 15.00),
    "gpt4_turbo": (10.00, 30.00),
    "claude_opus": (15.00, 75.00),
}


class CostManager:
    """σ-driven **lab** router: pick a placeholder-priced model; track token spend in USD."""

    def __init__(
        self,
        gate: Any = None,
        budget: float = 10.0,
        models: Optional[Dict[str, Tuple[float, float]]] = None,
    ) -> None:
        self.gate = gate if gate is not None else SigmaGate()
        self.budget = float(budget)
        self.spent = 0.0
        self.models: Dict[str, Tuple[float, float]] = dict(models) if models is not None else dict(MODEL_COSTS)
        self.history: List[Dict[str, Any]] = []

    def select_model(self, prompt: str, sigma_pre: Optional[float] = None) -> Dict[str, Any]:
        """Pick the cheapest **policy** tier for ``σ_pre``; if budget is exhausted, force a $0 model."""
        if sigma_pre is None:
            sg, _v = self.gate.score("route", str(prompt))
            sigma_pre = float(sg)
        else:
            sigma_pre = float(sigma_pre)

        if sigma_pre < 0.2:
            model = "local_3b"
        elif sigma_pre < 0.4:
            model = "qwen3_a3b"
        elif sigma_pre < 0.6:
            model = "gpt4o_mini"
        elif sigma_pre < 0.8:
            model = "gpt4o"
        else:
            model = "claude_opus"

        c_in, _c_out = self.models.get(model, (0.0, 0.0))
        if self.remaining() <= 0.0 and float(c_in) > 0.0:
            model = "local_7b"

        ci, co = self.models.get(model, (0.0, 0.0))
        return {
            "model": model,
            "sigma_pre": round(float(sigma_pre), 4),
            "cost_per_1M_in": float(ci),
            "cost_per_1M_out": float(co),
            "budget_remaining": round(float(self.remaining()), 4),
        }

    def record(self, model: str, input_tokens: int, output_tokens: int) -> Dict[str, Any]:
        costs = self.models.get(str(model), (0.0, 0.0))
        cost = float(input_tokens) * float(costs[0]) / 1_000_000.0 + float(output_tokens) * float(costs[1]) / 1_000_000.0
        self.spent += cost
        entry: Dict[str, Any] = {
            "model": str(model),
            "input_tokens": int(input_tokens),
            "output_tokens": int(output_tokens),
            "cost": round(float(cost), 6),
            "cumulative": round(float(self.spent), 4),
            "timestamp": time.time(),
        }
        self.history.append(entry)
        return entry

    def remaining(self) -> float:
        return max(0.0, float(self.budget) - float(self.spent))

    def reset(self) -> None:
        """Clear spend and history (in-process session)."""
        self.spent = 0.0
        self.history.clear()

    def summary(self) -> Dict[str, Any]:
        by_model: Dict[str, Dict[str, Any]] = {}
        for h in self.history:
            m = str(h["model"])
            row = by_model.setdefault(m, {"calls": 0, "cost": 0.0, "tokens": 0})
            row["calls"] += 1
            row["cost"] += float(h["cost"])
            row["tokens"] += int(h["input_tokens"]) + int(h["output_tokens"])

        n = len(self.history)
        local_ratio = (
            sum(1 for h in self.history if float(self.models.get(str(h["model"]), (0.0, 0.0))[0]) == 0.0) / float(max(n, 1))
        )
        return {
            "total_spent": round(float(self.spent), 4),
            "budget": float(self.budget),
            "remaining": round(float(self.remaining()), 4),
            "total_calls": n,
            "local_ratio": round(float(local_ratio), 4),
            "savings_vs_all_opus_percent": self._savings_vs_opus(),
            "by_model": by_model,
            "disclaimer": "Placeholder tariffs — not billing; savings are vs claude_opus table, lab only.",
        }

    def _savings_vs_opus(self) -> float:
        """Percent saved vs replaying history at ``claude_opus`` list prices."""
        opus_rates = self.models.get("claude_opus", (15.0, 75.0))
        opus_cost = sum(
            float(h["input_tokens"]) * float(opus_rates[0]) / 1_000_000.0
            + float(h["output_tokens"]) * float(opus_rates[1]) / 1_000_000.0
            for h in self.history
        )
        if opus_cost <= 0.0:
            return 0.0
        return round((1.0 - float(self.spent) / opus_cost) * 100.0, 1)


class SigmaCost:
    """Token-cost model + cascade / fleet hooks (lab)."""

    def __init__(
        self,
        gate: Any = None,
        *,
        eur_per_1k_tokens: Optional[Dict[str, float]] = None,
    ) -> None:
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
