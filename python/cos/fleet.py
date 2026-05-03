# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-fleet — register models, σ-skewed routing, cascade, and cost log (lab).

Does not talk to a live registry service. See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

from typing import Any, Dict, List, Optional

__all__ = ["SigmaFleet"]


class SigmaFleet:
    """Named models with cost/latency/σ anchors; cascade picks first acceptable."""

    def __init__(self) -> None:
        self.models: Dict[str, Dict[str, Any]] = {}
        self._cost_log: List[Dict[str, Any]] = []

    def register(
        self,
        name: str,
        *,
        model: Any = None,
        avg_sigma: float = 0.35,
        cost: float = 1.0,
        latency_ms: float = 100.0,
    ) -> None:
        self.models[str(name)] = {
            "model": model,
            "avg_sigma": float(avg_sigma),
            "cost": float(cost),
            "latency_ms": float(latency_ms),
        }

    def route(self, query: str, gate: Any) -> Dict[str, Any]:
        if not self.models:
            return {"name": None, "sigma": 1.0, "model": None}
        best: Optional[tuple[float, str, float]] = None
        for name, meta in self.models.items():
            s = float(gate.compute_sigma(None, None, str(query), f"model:{name}"))
            score = s * 0.6 + float(meta["cost"]) * 0.2
            if best is None or score < best[0]:
                best = (score, name, s)
        if best is None:
            return {"name": None, "sigma": 1.0, "model": None}
        _sc, name, sig = best
        return {
            "name": name,
            "sigma": round(sig, 6),
            "model": self.models[name]["model"],
        }

    def cascade_route(self, query: str, gate: Any) -> Dict[str, Any]:
        if not self.models:
            return {"name": None, "sigma": 1.0, "model": None, "cascade_stop": False}
        names = sorted(self.models.keys(), key=lambda n: self.models[n]["cost"])
        ta = float(gate.threshold_accept)
        for n in names:
            s = float(gate.compute_sigma(None, None, str(query), f"cascade:{n}"))
            if s < ta:
                return {
                    "name": n,
                    "sigma": round(s, 6),
                    "model": self.models[n]["model"],
                    "cascade_stop": True,
                }
        n = names[-1] if names else None
        if n is None:
            return {"name": None, "sigma": 1.0, "model": None, "cascade_stop": False}
        s = float(gate.compute_sigma(None, None, str(query), f"cascade:{n}"))
        return {
            "name": n,
            "sigma": round(s, 6),
            "model": self.models[n]["model"],
            "cascade_stop": False,
        }

    def cost_tracking(self, name: str, units: float) -> None:
        self._cost_log.append({"model": str(name), "units": float(units)})

    def sigma_portfolio(self) -> Dict[str, float]:
        return {n: float(v["avg_sigma"]) for n, v in self.models.items()}

    def auto_fallback(self, query: str, gate: Any, order: List[str]) -> Dict[str, Any]:
        for n in order:
            if n not in self.models:
                continue
            s = float(gate.compute_sigma(None, None, str(query), f"fallback:{n}"))
            if s < 0.85:
                return {
                    "name": n,
                    "sigma": round(s, 6),
                    "model": self.models[n]["model"],
                }
        return {"name": None, "sigma": 1.0, "model": None}
