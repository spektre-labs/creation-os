# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-MoE — inference-time mixture-of-experts style routing using σ-gate scores.

Each **expert** is a lab hook: a callable (``model.generate``-shaped), a cost weight,
optional domain tags, and a σ band ``[min_sigma, max_sigma]`` where the expert is
eligible. No training is required; it complements other lab routers (e.g. probe
cascade) when those modules are present. Heuristic only — do not merge with measured
harness headlines; see
``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import time
from typing import Any, Callable, Dict, List, Optional


class Expert:
    """One expert in the pool (callable strategy, cost, σ band, optional domains)."""

    def __init__(
        self,
        name: str,
        fn: Optional[Callable[..., str]] = None,
        cost: float = 1.0,
        domains: Optional[List[str]] = None,
        min_sigma: float = 0.0,
        max_sigma: float = 1.0,
    ) -> None:
        self.name = str(name)
        self.fn = fn
        self.cost = float(cost)
        self.domains = list(domains or [])
        self.min_sigma = float(min_sigma)
        self.max_sigma = float(max_sigma)
        self.stats: Dict[str, Any] = {"calls": 0, "accepts": 0, "total_sigma": 0.0}

    def __repr__(self) -> str:
        return f"Expert({self.name!r}, cost={self.cost})"


class SigmaMoE:
    """σ-guided expert selection: cascade, parallel, or budget-limited traversal."""

    def __init__(self, gate: Any = None) -> None:
        from cos.sigma_gate import SigmaGate

        self.gate = gate or SigmaGate()
        self.experts: List[Expert] = []
        self.routing_history: List[Dict[str, Any]] = []

    def add_expert(
        self,
        name: str,
        fn: Optional[Callable[..., str]] = None,
        **kwargs: Any,
    ) -> Expert:
        expert = Expert(name, fn=fn, **kwargs)
        self.experts.append(expert)
        return expert

    def route(
        self,
        prompt: str,
        response: Optional[str] = None,
        sigma: Optional[float] = None,
        budget: Optional[float] = None,
        strategy: str = "cascade",
        domain: Optional[str] = None,
    ) -> Dict[str, Any]:
        t0 = time.monotonic()

        if response is not None and sigma is None:
            sigma, _ = self.gate.score(str(prompt), str(response))
            sigma = float(sigma)

        if strategy == "cascade":
            result = self._cascade(prompt, response, sigma, budget, domain)
        elif strategy == "parallel":
            result = self._parallel(prompt, response, budget, domain)
        elif strategy == "budget":
            result = self._budget_aware(prompt, response, float(budget or 1.0), domain)
        else:
            result = self._cascade(prompt, response, sigma, budget, domain)

        result["elapsed_ms"] = round((time.monotonic() - t0) * 1000, 2)
        self.routing_history.append(dict(result))
        return result

    def _sigma_ok(self, expert: Expert, sigma: Optional[float]) -> bool:
        if sigma is None:
            return True
        s = float(sigma)
        return expert.min_sigma <= s <= expert.max_sigma

    def _domain_ok(self, expert: Expert, domain: Optional[str]) -> bool:
        if not domain or not expert.domains:
            return True
        return domain in expert.domains

    def _cascade(
        self,
        prompt: str,
        response: Optional[str],
        sigma: Optional[float],
        budget: Optional[float],
        domain: Optional[str],
    ) -> Dict[str, Any]:
        sorted_experts = sorted(self.experts, key=lambda e: e.cost)
        best: Optional[Dict[str, Any]] = None
        trace: List[Dict[str, Any]] = []

        for expert in sorted_experts:
            if budget is not None and expert.cost > float(budget):
                continue
            if not self._sigma_ok(expert, sigma):
                continue
            if not self._domain_ok(expert, domain):
                continue

            if expert.fn is not None and response is None:
                resp = expert.fn(prompt)
            else:
                resp = response

            if resp:
                s, v = self.gate.score(str(prompt), str(resp))
            else:
                s, v = (float(sigma) if sigma is not None else 0.5), "RETHINK"

            expert.stats["calls"] += 1
            expert.stats["total_sigma"] += float(s)

            entry = {
                "expert": expert.name,
                "sigma": round(float(s), 4),
                "verdict": str(v),
                "cost": expert.cost,
            }
            trace.append(entry)

            if v == "ACCEPT":
                expert.stats["accepts"] += 1
                best = {
                    "expert": expert.name,
                    "response": resp,
                    "sigma": round(float(s), 4),
                    "verdict": str(v),
                    "cost": expert.cost,
                    "trace": trace,
                }
                break

        if best is not None:
            return best

        if trace:
            best_entry = min(trace, key=lambda t: float(t["sigma"]))
            return {
                "expert": best_entry["expert"],
                "response": response,
                "sigma": float(best_entry["sigma"]),
                "verdict": str(best_entry["verdict"]),
                "cost": float(best_entry["cost"]),
                "trace": trace,
            }

        return {
            "expert": None,
            "response": response,
            "sigma": 1.0,
            "verdict": "NO_EXPERT",
            "cost": 0.0,
            "trace": [],
        }

    def _parallel(
        self,
        prompt: str,
        response: Optional[str],
        budget: Optional[float],
        domain: Optional[str],
    ) -> Dict[str, Any]:
        results: List[Dict[str, Any]] = []

        for expert in self.experts:
            if budget is not None and expert.cost > float(budget):
                continue
            if not self._domain_ok(expert, domain):
                continue

            if expert.fn is not None and response is None:
                resp = expert.fn(prompt)
            else:
                resp = response

            if resp:
                s, v = self.gate.score(str(prompt), str(resp))
            else:
                s, v = 0.5, "RETHINK"

            expert.stats["calls"] += 1
            expert.stats["total_sigma"] += float(s)
            if v == "ACCEPT":
                expert.stats["accepts"] += 1

            results.append(
                {
                    "expert": expert.name,
                    "response": resp,
                    "sigma": round(float(s), 4),
                    "verdict": str(v),
                    "cost": expert.cost,
                }
            )

        if results:
            best = min(results, key=lambda r: float(r["sigma"]))
            out = dict(best)
            out["trace"] = results
            return out

        return {
            "expert": None,
            "sigma": 1.0,
            "verdict": "NO_EXPERT",
            "trace": [],
            "cost": 0.0,
        }

    def _budget_aware(
        self,
        prompt: str,
        response: Optional[str],
        budget: float,
        domain: Optional[str],
    ) -> Dict[str, Any]:
        remaining = float(budget)
        trace: List[Dict[str, Any]] = []
        best: Optional[Dict[str, Any]] = None

        for expert in sorted(self.experts, key=lambda e: e.cost):
            if expert.cost > remaining:
                continue
            if not self._domain_ok(expert, domain):
                continue

            remaining -= expert.cost

            if expert.fn is not None and response is None:
                resp = expert.fn(prompt)
            else:
                resp = response

            if resp:
                s, v = self.gate.score(str(prompt), str(resp))
            else:
                s, v = 0.5, "RETHINK"

            expert.stats["calls"] += 1
            expert.stats["total_sigma"] += float(s)

            trace.append(
                {
                    "expert": expert.name,
                    "sigma": round(float(s), 4),
                    "verdict": str(v),
                    "cost": expert.cost,
                    "budget_remaining": round(remaining, 2),
                }
            )

            if best is None or float(s) < float(best["sigma"]):
                best = {
                    "expert": expert.name,
                    "response": resp,
                    "sigma": round(float(s), 4),
                    "verdict": str(v),
                    "cost": expert.cost,
                }

            if v == "ACCEPT":
                break

        if best:
            out = dict(best)
            out["trace"] = trace
            out["budget_used"] = round(float(budget) - remaining, 2)
            return out

        return {
            "expert": None,
            "sigma": 1.0,
            "verdict": "NO_EXPERT",
            "trace": trace,
            "cost": 0.0,
            "budget_used": 0.0,
        }

    def expert_stats(self) -> Dict[str, Dict[str, Any]]:
        return {
            e.name: {
                "calls": e.stats["calls"],
                "accepts": e.stats["accepts"],
                "accept_rate": e.stats["accepts"] / max(e.stats["calls"], 1),
                "avg_sigma": e.stats["total_sigma"] / max(e.stats["calls"], 1),
                "cost": e.cost,
            }
            for e in self.experts
        }

    def routing_signature(self, n_last: int = 10) -> Dict[str, float]:
        """Fraction of recent routings that landed on each expert name (lab monitor)."""
        recent = self.routing_history[-n_last:]
        if not recent:
            return {}

        counts: Dict[str, int] = {}
        for r in recent:
            name = r.get("expert") or "none"
            counts[str(name)] = counts.get(str(name), 0) + 1

        total = len(recent)
        return {k: round(v / total, 2) for k, v in counts.items()}


__all__ = ["Expert", "SigmaMoE"]
