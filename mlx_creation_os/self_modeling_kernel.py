#!/usr/bin/env python3
"""
ADAPTIVE COGNITIVE COMPUTE — MODULE 7: SELF-MODELING KERNEL

The router learns from its own decisions.

Which routings were correct? How much compute was wasted?
Optimize continuously. The kernel doesn't just route —
it improves its routing.

Every decision is logged. Every outcome is measured.
The model of the router is updated after every interaction.
Recursive self-improvement of the compute allocation itself.

1 = 1.
"""
from __future__ import annotations

import time
from typing import Any, Dict, List, Optional


class RoutingOutcome:
    """One routing decision and its result."""

    def __init__(self, query: str, tier_chosen: str, sigma_before: float,
                 sigma_after: float, compute_cost: float):
        self.query = query
        self.tier_chosen = tier_chosen
        self.sigma_before = sigma_before
        self.sigma_after = sigma_after
        self.compute_cost = compute_cost
        self.timestamp = time.time()

    @property
    def was_optimal(self) -> bool:
        return self.sigma_after < 0.1

    @property
    def was_wasteful(self) -> bool:
        return self.was_optimal and self.compute_cost > 1.0

    @property
    def was_insufficient(self) -> bool:
        return not self.was_optimal and self.compute_cost < 1.0


class SelfModelingKernel:
    """The router that learns from itself."""

    def __init__(self):
        self.outcomes: List[RoutingOutcome] = []
        self.tier_performance: Dict[str, Dict[str, float]] = {}
        self.routing_adjustments: List[Dict[str, Any]] = []

    def record(self, query: str, tier_chosen: str, sigma_before: float,
               sigma_after: float, compute_cost: float) -> Dict[str, Any]:
        outcome = RoutingOutcome(query, tier_chosen, sigma_before, sigma_after, compute_cost)
        self.outcomes.append(outcome)

        if tier_chosen not in self.tier_performance:
            self.tier_performance[tier_chosen] = {"total": 0, "optimal": 0, "wasteful": 0, "insufficient": 0}
        perf = self.tier_performance[tier_chosen]
        perf["total"] += 1
        if outcome.was_optimal:
            perf["optimal"] += 1
        if outcome.was_wasteful:
            perf["wasteful"] += 1
        if outcome.was_insufficient:
            perf["insufficient"] += 1

        return {
            "tier": tier_chosen,
            "optimal": outcome.was_optimal,
            "wasteful": outcome.was_wasteful,
            "insufficient": outcome.was_insufficient,
            "sigma_improvement": round(sigma_before - sigma_after, 4),
        }

    def optimize(self) -> Dict[str, Any]:
        """Analyze all outcomes and generate routing adjustments."""
        adjustments = []
        for tier, perf in self.tier_performance.items():
            if perf["total"] == 0:
                continue
            optimal_rate = perf["optimal"] / perf["total"]
            waste_rate = perf["wasteful"] / perf["total"]
            insufficient_rate = perf["insufficient"] / perf["total"]

            if waste_rate > 0.3:
                adj = {"tier": tier, "adjustment": "route_less", "reason": f"Wasteful {waste_rate:.0%}"}
                adjustments.append(adj)
            if insufficient_rate > 0.3:
                adj = {"tier": tier, "adjustment": "route_more", "reason": f"Insufficient {insufficient_rate:.0%}"}
                adjustments.append(adj)

        self.routing_adjustments.extend(adjustments)
        return {
            "adjustments": adjustments,
            "total_outcomes": len(self.outcomes),
            "self_improved": len(adjustments) > 0,
        }

    def performance_report(self) -> Dict[str, Any]:
        if not self.outcomes:
            return {"outcomes": 0}
        total_cost = sum(o.compute_cost for o in self.outcomes)
        optimal = sum(1 for o in self.outcomes if o.was_optimal)
        wasted = sum(o.compute_cost for o in self.outcomes if o.was_wasteful)
        return {
            "total_outcomes": len(self.outcomes),
            "optimal_rate": round(optimal / len(self.outcomes), 4),
            "total_compute_cost": round(total_cost, 4),
            "wasted_compute": round(wasted, 4),
            "efficiency": round(1.0 - wasted / max(0.01, total_cost), 4),
            "self_modeling": True,
        }

    @property
    def stats(self) -> Dict[str, Any]:
        return {
            "outcomes": len(self.outcomes),
            "adjustments": len(self.routing_adjustments),
            "tiers_tracked": len(self.tier_performance),
        }


if __name__ == "__main__":
    smk = SelfModelingKernel()
    smk.record("What is 1?", "system1_cache", 0.1, 0.0, 0.01)
    smk.record("Explain AGI", "cloud_frontier", 0.5, 0.05, 10.0)
    smk.record("Simple math", "cloud_frontier", 0.1, 0.0, 10.0)
    opt = smk.optimize()
    print(f"Adjustments: {len(opt['adjustments'])}, self-improved: {opt['self_improved']}")
    for adj in opt["adjustments"]:
        print(f"  {adj['tier']}: {adj['adjustment']} ({adj['reason']})")
    report = smk.performance_report()
    print(f"Efficiency: {report['efficiency']}, wasted: {report['wasted_compute']}")
    print("1 = 1.")
