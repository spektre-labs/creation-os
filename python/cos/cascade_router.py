# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Model-routing cascade: try cheap tiers first, escalate on high σ.

This is a **cost / capability orchestration layer** on top of any backends — not a
foundational LM. Throughput, savings, and AUROC headlines bind to ``docs/CLAIM_DISCIPLINE.md``
and archived harness rows, not to this lab router alone.

Note: ``cos.cascade`` (same package) implements **L2–L6 hidden-state signal helpers** for
:class:`~cos.sigma_gate.SigmaGate`. This module is **multi-model routing** (FAST → VERIFY → …)."""
from __future__ import annotations

import time
from typing import Any, Callable, Dict, List, Optional

from cos.sigma_gate import SigmaGate

__all__ = ["CascadeLevel", "SigmaCascade"]


def _norm_verdict(verdict: Any) -> str:
    raw = str(getattr(verdict, "name", verdict))
    return raw.split(".")[-1].upper() if "." in raw else raw.upper()


class CascadeLevel:
    """One tier: name, optional ``model_fn(prompt) -> str``, cost, and σ ceiling."""

    def __init__(
        self,
        name: str,
        model_fn: Optional[Callable[[str], str]] = None,
        cost_per_call: float = 0.0,
        max_σ: float = 0.3,
    ) -> None:
        self.name = str(name)
        self.model_fn = model_fn
        self.cost_per_call = float(cost_per_call)
        self.max_σ = float(max_σ)
        self.calls = 0
        self.accepts = 0


class SigmaCascade:
    """σ-driven escalation across ordered tiers (cheapest first)."""

    def __init__(self, gate: Optional[Any] = None) -> None:
        self.gate = gate if gate is not None else SigmaGate()
        self.levels: List[CascadeLevel] = []
        self.total_queries = 0
        self.total_cost = 0.0

    def add_level(
        self,
        name: str,
        model_fn: Optional[Callable[[str], str]] = None,
        cost: float = 0.0,
        max_σ: float = 0.3,
    ) -> None:
        """Append a tier. Call in ascending cost order."""
        self.levels.append(CascadeLevel(name, model_fn, cost, max_σ))

    def route(self, prompt: str) -> Dict[str, Any]:
        """Try each tier until σ is within tier budget or verdict is ACCEPT."""
        self.total_queries += 1
        start = time.time()
        trace: List[Dict[str, Any]] = []

        for level in self.levels:
            level.calls += 1

            if level.model_fn is not None:
                response = level.model_fn(prompt)
            else:
                response = f"[{level.name}] response to: {str(prompt)[:50]}"

            σ, verdict = self.gate.score(str(prompt), str(response))
            vn = _norm_verdict(verdict)
            self.total_cost += level.cost_per_call

            trace.append(
                {
                    "level": level.name,
                    "σ": round(float(σ), 4),
                    "verdict": vn,
                    "cost": level.cost_per_call,
                }
            )

            if float(σ) <= level.max_σ or vn == "ACCEPT":
                level.accepts += 1
                latency_ms = (time.time() - start) * 1000.0
                return {
                    "response": response,
                    "σ": round(float(σ), 4),
                    "verdict": vn,
                    "resolved_at": level.name,
                    "levels_tried": len(trace),
                    "cost": round(sum(t["cost"] for t in trace), 6),
                    "latency_ms": round(latency_ms, 1),
                    "trace": trace,
                }

        latency_ms = (time.time() - start) * 1000.0
        return {
            "response": None,
            "σ": 1.0,
            "verdict": "ABSTAIN",
            "resolved_at": None,
            "levels_tried": len(trace),
            "cost": round(sum(t["cost"] for t in trace), 6),
            "latency_ms": round(latency_ms, 1),
            "trace": trace,
        }

    def stats(self) -> Dict[str, Any]:
        """Compare cumulative cost vs always-using the most expensive tier (baseline)."""
        baseline_cost = self.total_queries * (
            self.levels[-1].cost_per_call if self.levels else 0.0
        )
        savings = baseline_cost - self.total_cost
        denom = max(baseline_cost, 1e-9)

        return {
            "total_queries": self.total_queries,
            "total_cost": round(self.total_cost, 4),
            "baseline_cost": round(baseline_cost, 4),
            "savings": round(savings, 4),
            "savings_pct": round(savings / denom * 100.0, 1),
            "per_level": [
                {
                    "name": lev.name,
                    "calls": lev.calls,
                    "accepts": lev.accepts,
                    "accept_rate": round(lev.accepts / max(lev.calls, 1), 4),
                    "cost_per_call": lev.cost_per_call,
                }
                for lev in self.levels
            ],
        }

    @staticmethod
    def default_cascade(gate: Optional[Any] = None) -> "SigmaCascade":
        """Lab default: FAST → VERIFY → RAG → ESCALATE; exhaustion implies ABSTAIN in :meth:`route`.

        Costs are **illustrative dollars-units** for dashboards — rebind to your price book."""
        cascade = SigmaCascade(gate)
        cascade.add_level("FAST", cost=0.000, max_σ=0.15)
        cascade.add_level("VERIFY", cost=0.001, max_σ=0.30)
        cascade.add_level("RAG", cost=0.005, max_σ=0.50)
        cascade.add_level("ESCALATE", cost=0.010, max_σ=0.70)
        return cascade
