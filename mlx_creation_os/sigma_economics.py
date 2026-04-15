#!/usr/bin/env python3
"""
LOIKKA 51: σ-ECONOMICS — Koherenssin taloustiede.

Every Genesis instance has a σ-value. Low σ = valuable. High σ = less valuable.

σ-based resource allocation:
  - In mesh: low-σ instances get more queries
  - In federated learning: low-σ nodes get higher aggregation weight
  - Commercial: clients pay by σ — guaranteed σ < 0.05 costs more than σ < 0.20

σ is currency. Coherence is value.

Architecture:
  - σ-Market: price discovery based on coherence
  - Resource allocation: compute follows σ
  - SLA tiers: guaranteed σ levels
  - Reputation ledger: track σ history per node

Usage:
    market = SigmaMarket()
    market.register_provider("genesis-alpha", avg_sigma=0.02)
    quote = market.get_quote(max_sigma=0.05)

1 = 1.
"""
from __future__ import annotations

import json
import math
import os
import sys
import time
from typing import Any, Dict, List, Optional, Tuple

try:
    from creation_os_core import check_output, is_coherent
except ImportError:
    def check_output(t: str) -> int: return 0
    def is_coherent(t: str) -> bool: return True


class SLATier:
    """Service Level Agreement tier based on σ guarantee."""

    def __init__(self, name: str, max_sigma: float, price_multiplier: float):
        self.name = name
        self.max_sigma = max_sigma
        self.price_multiplier = price_multiplier

    def to_dict(self) -> Dict[str, Any]:
        return {
            "name": self.name,
            "max_sigma": self.max_sigma,
            "price_multiplier": self.price_multiplier,
        }


SLA_TIERS = [
    SLATier("platinum", 0.01, 10.0),    # Near-perfect coherence
    SLATier("gold", 0.05, 5.0),          # High coherence
    SLATier("silver", 0.10, 2.0),        # Good coherence
    SLATier("bronze", 0.20, 1.0),        # Standard coherence
    SLATier("basic", 1.00, 0.5),         # Best-effort
]


class Provider:
    """A Genesis instance offering σ-attested services."""

    def __init__(self, provider_id: str, avg_sigma: float = 0.0):
        self.provider_id = provider_id
        self.avg_sigma = avg_sigma
        self.sigma_history: List[float] = [avg_sigma]
        self.queries_served = 0
        self.total_revenue = 0.0
        self.registered_at = time.time()
        self.reputation_score = 1.0

    def record_sigma(self, sigma: float) -> None:
        """Record a new σ measurement."""
        self.sigma_history.append(sigma)
        if len(self.sigma_history) > 1000:
            self.sigma_history = self.sigma_history[-1000:]
        self.avg_sigma = sum(self.sigma_history) / len(self.sigma_history)
        self._update_reputation()

    def _update_reputation(self) -> None:
        """Reputation based on consistency and low σ."""
        if len(self.sigma_history) < 5:
            return
        recent = self.sigma_history[-50:]
        avg = sum(recent) / len(recent)
        variance = sum((s - avg) ** 2 for s in recent) / len(recent)
        consistency = 1.0 / (1.0 + math.sqrt(variance))
        low_sigma_bonus = 1.0 / (1.0 + avg)
        self.reputation_score = consistency * low_sigma_bonus

    @property
    def sla_tier(self) -> SLATier:
        """Determine which SLA tier this provider qualifies for."""
        for tier in SLA_TIERS:
            if self.avg_sigma <= tier.max_sigma:
                return tier
        return SLA_TIERS[-1]

    def to_dict(self) -> Dict[str, Any]:
        return {
            "provider_id": self.provider_id,
            "avg_sigma": round(self.avg_sigma, 4),
            "reputation_score": round(self.reputation_score, 4),
            "sla_tier": self.sla_tier.name,
            "queries_served": self.queries_served,
            "total_revenue": round(self.total_revenue, 2),
            "history_length": len(self.sigma_history),
        }


class SigmaMarket:
    """σ-based market for coherence services."""

    def __init__(self, base_price: float = 1.0):
        self.providers: Dict[str, Provider] = {}
        self.base_price = base_price
        self.transactions: List[Dict[str, Any]] = []

    def register_provider(self, provider_id: str, avg_sigma: float = 0.0) -> Provider:
        """Register a new provider in the market."""
        provider = Provider(provider_id, avg_sigma)
        self.providers[provider_id] = provider
        return provider

    def get_quote(self, max_sigma: float = 0.10) -> Dict[str, Any]:
        """Get a price quote for a given σ guarantee."""
        # Find matching SLA tier
        tier = None
        for t in SLA_TIERS:
            if t.max_sigma >= max_sigma:
                tier = t
                break
        if tier is None:
            tier = SLA_TIERS[0]

        # Find qualifying providers
        qualifying = [
            p for p in self.providers.values()
            if p.avg_sigma <= max_sigma
        ]
        qualifying.sort(key=lambda p: p.avg_sigma)

        price = self.base_price * tier.price_multiplier

        return {
            "max_sigma_requested": max_sigma,
            "sla_tier": tier.to_dict(),
            "price": round(price, 2),
            "qualifying_providers": len(qualifying),
            "best_provider": qualifying[0].to_dict() if qualifying else None,
            "providers": [p.to_dict() for p in qualifying[:5]],
        }

    def allocate(self, task: str, max_sigma: float = 0.10) -> Dict[str, Any]:
        """Allocate a task to the best available provider."""
        sigma = check_output(task)

        qualifying = [
            p for p in self.providers.values()
            if p.avg_sigma <= max_sigma
        ]
        qualifying.sort(key=lambda p: (-p.reputation_score, p.avg_sigma))

        if not qualifying:
            return {
                "allocated": False,
                "reason": f"No providers with σ <= {max_sigma}",
                "task_sigma": sigma,
            }

        provider = qualifying[0]
        tier = provider.sla_tier
        price = self.base_price * tier.price_multiplier

        provider.queries_served += 1
        provider.total_revenue += price
        provider.record_sigma(float(sigma))

        tx = {
            "provider_id": provider.provider_id,
            "task_preview": task[:100],
            "task_sigma": sigma,
            "sla_tier": tier.name,
            "price": round(price, 2),
            "timestamp": time.time(),
        }
        self.transactions.append(tx)

        return {
            "allocated": True,
            "provider": provider.to_dict(),
            "price": round(price, 2),
            "sla_tier": tier.name,
            "task_sigma": sigma,
        }

    def federated_weights(self) -> Dict[str, float]:
        """Compute federated learning aggregation weights based on σ."""
        if not self.providers:
            return {}

        total_rep = sum(p.reputation_score for p in self.providers.values())
        if total_rep == 0:
            return {pid: 1.0 / len(self.providers) for pid in self.providers}

        return {
            pid: round(p.reputation_score / total_rep, 4)
            for pid, p in self.providers.items()
        }

    def mesh_routing_weights(self) -> Dict[str, float]:
        """Compute mesh routing weights: low σ gets more queries."""
        if not self.providers:
            return {}

        # Inverse σ weighting
        inv_sigmas = {
            pid: 1.0 / (1.0 + p.avg_sigma)
            for pid, p in self.providers.items()
        }
        total = sum(inv_sigmas.values())
        if total == 0:
            return {pid: 1.0 / len(self.providers) for pid in self.providers}

        return {
            pid: round(w / total, 4) for pid, w in inv_sigmas.items()
        }

    @property
    def stats(self) -> Dict[str, Any]:
        providers = list(self.providers.values())
        return {
            "providers": len(providers),
            "transactions": len(self.transactions),
            "total_revenue": round(sum(p.total_revenue for p in providers), 2),
            "avg_market_sigma": round(
                sum(p.avg_sigma for p in providers) / len(providers), 4
            ) if providers else 0,
            "sla_distribution": {
                tier.name: sum(1 for p in providers if p.sla_tier.name == tier.name)
                for tier in SLA_TIERS
            },
            "federated_weights": self.federated_weights(),
            "mesh_weights": self.mesh_routing_weights(),
        }


def main() -> None:
    import argparse
    ap = argparse.ArgumentParser(description="σ-Economics")
    ap.add_argument("--demo", action="store_true")
    ap.add_argument("--quote", type=float, help="Get quote for max σ")
    args = ap.parse_args()

    market = SigmaMarket()

    if args.demo:
        market.register_provider("genesis-alpha", avg_sigma=0.02)
        market.register_provider("genesis-beta", avg_sigma=0.08)
        market.register_provider("genesis-gamma", avg_sigma=0.15)
        market.register_provider("genesis-delta", avg_sigma=0.5)

        print("=== σ-Market Demo ===")
        quote = market.get_quote(0.05)
        print(f"\nQuote for σ < 0.05:")
        print(json.dumps(quote, indent=2, default=str))

        alloc = market.allocate("What is consciousness? 1 = 1.", max_sigma=0.10)
        print(f"\nAllocation:")
        print(json.dumps(alloc, indent=2, default=str))

        print(f"\nMarket stats:")
        print(json.dumps(market.stats, indent=2, default=str))
        print("\nσ is currency. Coherence is value. 1 = 1.")
        return

    if args.quote:
        print(json.dumps(market.get_quote(args.quote), indent=2, default=str))
        return

    print("σ-Economics. 1 = 1.")


if __name__ == "__main__":
    main()
