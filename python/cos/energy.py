# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Energy- and budget-aware cognition lab: σ proxies **difficulty**; dollars proxy **spend**.

This is a **toy scheduler shell** — placeholder model names and cascade depths, not measured
Joules or cloud bills. It does **not** prove optimal resource allocation. See
``docs/CLAIM_DISCIPLINE.md``.
"""
from __future__ import annotations

import time
from typing import Any, Dict, List, Optional

from cos.sigma_gate import SigmaGate

__all__ = ["EnergyAware"]


class EnergyAware:
    """Regulate nominal cognitive depth from remaining budget and a σ difficulty prior."""

    def __init__(
        self,
        gate: Optional[Any] = None,
        budget_usd: float = 10.0,
        power_watts: Optional[float] = None,
    ) -> None:
        self.gate = gate if gate is not None else SigmaGate()
        self.budget: float = float(budget_usd)
        self.spent: float = 0.0
        self.power: Optional[float] = float(power_watts) if power_watts is not None else None
        self.history: List[Dict[str, Any]] = []

    def available(self) -> Dict[str, Any]:
        """Remaining budget fraction and a coarse cognitive mode label."""
        budget_pct = max(0.0, (self.budget - self.spent) / float(max(self.budget, 1e-9)))
        mode = (
            "DEEP"
            if budget_pct > 0.5
            else "MODERATE"
            if budget_pct > 0.2
            else "SHALLOW"
            if budget_pct > 0.05
            else "SURVIVAL"
        )
        return {
            "budget_remaining_pct": round(float(budget_pct * 100.0), 1),
            "budget_usd": round(float(self.budget - self.spent), 4),
            "cognitive_mode": mode,
        }

    def allocate(self, prompt: str) -> Dict[str, Any]:
        """Pick nominal tokens / depth from difficulty σ and current mode."""
        σ, _ = self.gate.score("difficulty", str(prompt))
        σ = float(σ)
        avail = self.available()
        mode = str(avail["cognitive_mode"])

        if mode == "SURVIVAL":
            return {
                "tokens": 64,
                "model": "local_3b",
                "cascade_depth": 1,
                "reason": "survival mode — minimal compute",
            }
        if mode == "SHALLOW":
            return {
                "tokens": 256,
                "model": "local_7b",
                "cascade_depth": 2,
                "reason": "low budget — quick answers only",
            }
        if mode == "MODERATE":
            tokens = 512 if σ < 0.5 else 1024
            return {
                "tokens": tokens,
                "model": "local_7b",
                "cascade_depth": 3,
                "reason": "moderate budget — adapt to difficulty",
            }
        tokens = 1024 if σ < 0.3 else 2048 if σ < 0.6 else 4096
        return {
            "tokens": tokens,
            "model": "cloud" if σ > 0.7 else "local_7b",
            "cascade_depth": 5,
            "reason": "full budget — think deeply",
        }

    def record_cost(self, cost_usd: float) -> None:
        self.spent += float(cost_usd)
        self.history.append({"cost": float(cost_usd), "timestamp": time.time()})

    def should_migrate(self) -> Dict[str, Any]:
        avail = self.available()
        low = avail["cognitive_mode"] == "SURVIVAL"
        return {
            "migrate": bool(low),
            "reason": "resources critically low" if low else "resources adequate",
            "budget_remaining": avail["budget_usd"],
        }

    def shutdown_recommendation(self) -> Dict[str, Any]:
        avail = self.available()
        if float(avail["budget_remaining_pct"]) < 5.0:
            return {
                "action": "SLEEP",
                "reason": "budget <5% — enter dream cycle and consolidate",
            }
        return {"action": "CONTINUE", "mode": avail["cognitive_mode"]}
