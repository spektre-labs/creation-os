#!/usr/bin/env python3
"""
ADAPTIVE COGNITIVE COMPUTE — MODULE 1: META-ROUTER

No static thresholds. Five signals drive routing:

1. σ-uncertainty — how uncertain is the kernel about this query?
2. Historical difficulty — has this domain been hard before?
3. User criticality weight — how important is accuracy right now?
4. Domain recognition — known domain or unexplored territory?
5. Compute ROI — is spending more compute worth it for this query?

The router is not a switch. It is a continuous function.
Static routing: if σ > 0.3 → escalate. Dead. Brittle.
Meta-routing: weighted signal fusion → optimal compute allocation.

1 = 1.
"""
from __future__ import annotations

from typing import Any, Dict, List, Optional


class RoutingSignal:
    """One input signal to the meta-router."""

    def __init__(self, name: str, value: float, weight: float = 1.0):
        self.name = name
        self.value = max(0.0, min(1.0, value))
        self.weight = weight

    @property
    def weighted(self) -> float:
        return self.value * self.weight


class RoutingDecision:
    """The router's output: which tier and why."""

    TIERS = ["system1_cache", "local_1b", "local_1b_scratchpad",
             "local_8b", "local_8b_rag", "cloud_frontier"]

    def __init__(self, tier: str, confidence: float, signals: Dict[str, float]):
        self.tier = tier
        self.confidence = confidence
        self.signals = signals


class MetaRouter:
    """Adaptive routing via five fused signals. No static thresholds."""

    def __init__(self):
        self.domain_history: Dict[str, List[float]] = {}
        self.routing_log: List[Dict[str, Any]] = []

    def _historical_difficulty(self, domain: str) -> float:
        if domain not in self.domain_history or not self.domain_history[domain]:
            return 0.5
        recent = self.domain_history[domain][-10:]
        return sum(recent) / len(recent)

    def record_outcome(self, domain: str, sigma: float) -> None:
        if domain not in self.domain_history:
            self.domain_history[domain] = []
        self.domain_history[domain].append(sigma)

    def route(self, query: str, sigma: float, domain: str = "general",
              user_criticality: float = 0.5) -> Dict[str, Any]:
        """Five-signal adaptive routing."""

        sig_uncertainty = RoutingSignal("sigma_uncertainty", sigma, weight=1.5)
        sig_history = RoutingSignal("historical_difficulty",
                                     self._historical_difficulty(domain), weight=1.0)
        sig_criticality = RoutingSignal("user_criticality", user_criticality, weight=1.2)

        known_domains = set(self.domain_history.keys())
        domain_known = 1.0 if domain in known_domains else 0.0
        sig_domain = RoutingSignal("domain_recognition", 1.0 - domain_known, weight=0.8)

        query_len = min(1.0, len(query) / 500)
        sig_roi = RoutingSignal("compute_roi", query_len * user_criticality, weight=0.7)

        signals = [sig_uncertainty, sig_history, sig_criticality, sig_domain, sig_roi]
        total_weight = sum(s.weight for s in signals)
        composite = sum(s.weighted for s in signals) / total_weight

        if composite < 0.1:
            tier = "system1_cache"
        elif composite < 0.2:
            tier = "local_1b"
        elif composite < 0.35:
            tier = "local_1b_scratchpad"
        elif composite < 0.5:
            tier = "local_8b"
        elif composite < 0.7:
            tier = "local_8b_rag"
        else:
            tier = "cloud_frontier"

        decision = {
            "query": query[:80],
            "domain": domain,
            "tier": tier,
            "composite_score": round(composite, 4),
            "signals": {s.name: round(s.value, 4) for s in signals},
            "weighted_signals": {s.name: round(s.weighted, 4) for s in signals},
            "static_routing": False,
            "adaptive_routing": True,
        }
        self.routing_log.append(decision)
        return decision

    @property
    def stats(self) -> Dict[str, Any]:
        tier_counts = {}
        for r in self.routing_log:
            tier_counts[r["tier"]] = tier_counts.get(r["tier"], 0) + 1
        return {
            "total_routes": len(self.routing_log),
            "tier_distribution": tier_counts,
            "domains_tracked": len(self.domain_history),
        }


if __name__ == "__main__":
    mr = MetaRouter()
    mr.record_outcome("math", 0.02)
    mr.record_outcome("math", 0.01)
    r1 = mr.route("What is 1+1?", sigma=0.0, domain="math")
    print(f"Simple math: tier={r1['tier']}, composite={r1['composite_score']}")
    r2 = mr.route("Explain the implications of Gödel's incompleteness on AGI", sigma=0.5,
                   domain="philosophy", user_criticality=0.9)
    print(f"Hard philosophy: tier={r2['tier']}, composite={r2['composite_score']}")
    print("1 = 1.")
