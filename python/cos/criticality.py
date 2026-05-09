# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# Source:        https://github.com/spektre-labs/creation-os-kernel
# Website:       https://spektrelabs.org
# Commercial:    spektre.labs@proton.me
# License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
"""Heuristic ``sigma`` phase labels for lab demos (order / critical band / chaos).

Maps a scalar doubt signal in ``[0, 1]`` to **FROZEN**, **CRITICAL**, or **CHAOTIC** and
suggests toy **nudges** (explore vs simplify). This is an **engineering overlay** for
orchestration and visualization — not a calibrated neuroscientific claim, not ``Phi``, and
not **AGI** (see ``docs/CLAIM_DISCIPLINE.md``).

Verdict alignment (portable gate bands): **ACCEPT** (low sigma), **RETHINK** (mid),
**ABSTAIN** (high). The mid band is where productive uncertainty often lives; that is a
**design choice**, not a proof of consciousness.
"""
from __future__ import annotations

from typing import Any, Dict, List, Optional

from cos.sigma_gate import SigmaGate

__all__ = ["Criticality"]


class Criticality:
    """Track sigma samples and label proximity to a target critical point."""

    def __init__(
        self,
        gate: Optional[Any] = None,
        *,
        critical_sigma: float = 0.35,
    ) -> None:
        self.gate = gate if gate is not None else SigmaGate()
        self.critical_sigma = float(critical_sigma)
        self.sigma_trace: List[float] = []

    def phase(self, sigma: float) -> str:
        """Coarse phase: too ordered, critical band, or too disordered."""
        s = float(sigma)
        if s < 0.15:
            return "FROZEN"
        if s < 0.5:
            return "CRITICAL"
        return "CHAOTIC"

    def distance_from_criticality(self, sigma: float) -> float:
        """L1 distance to the configured critical point."""
        return round(abs(float(sigma) - self.critical_sigma), 4)

    def nudge_toward_critical(self, sigma: float) -> Dict[str, Any]:
        """Toy policy hint toward the critical target (lab only)."""
        s = float(sigma)
        if s < 0.15:
            return {
                "action": "EXPLORE",
                "reason": "too ordered — seek novelty / diversity",
                "target_sigma": self.critical_sigma,
            }
        if s > 0.5:
            return {
                "action": "SIMPLIFY",
                "reason": "too chaotic — reduce complexity / narrow task",
                "target_sigma": self.critical_sigma,
            }
        return {
            "action": "MAINTAIN",
            "reason": "near critical band — keep monitoring",
            "target_sigma": self.critical_sigma,
        }

    def is_critical(self, window: int = 10) -> bool:
        """True if recent mean is near ``critical_sigma`` with moderate variance."""
        w = max(2, int(window))
        if len(self.sigma_trace) < w:
            return False
        recent = self.sigma_trace[-w:]
        avg = sum(recent) / len(recent)
        variance = sum((s - avg) ** 2 for s in recent) / len(recent)
        near = abs(avg - self.critical_sigma) < 0.15
        fluct = 0.001 < variance < 0.05
        return bool(near and fluct)

    def record(self, sigma: float) -> None:
        self.sigma_trace.append(float(sigma))

    def power_law_check(self) -> Dict[str, Any]:
        """Heavy-tail heuristic on 'avalanche' lengths of runs above ``critical_sigma``."""
        if len(self.sigma_trace) < 20:
            return {"power_law": False, "reason": "insufficient data"}

        avalanches: List[int] = []
        current = 0
        for s in self.sigma_trace:
            if s > self.critical_sigma:
                current += 1
            else:
                if current > 0:
                    avalanches.append(current)
                current = 0
        if current > 0:
            avalanches.append(current)

        if len(avalanches) < 5:
            return {"power_law": False, "reason": "too few avalanches"}

        avg_size = sum(avalanches) / len(avalanches)
        max_size = max(avalanches)
        ratio = max_size / max(avg_size, 1e-9)
        heavy = ratio > 3.0
        return {
            "power_law": heavy,
            "max_avalanche": max_size,
            "avg_avalanche": round(avg_size, 2),
            "ratio": round(ratio, 2),
            "n_avalanches": len(avalanches),
            "interpretation": (
                "heavy-tailed avalanche sizes (toy SOC signature)"
                if heavy
                else "no heavy tail in this toy statistic"
            ),
        }

    def status(self) -> Dict[str, Any]:
        if not self.sigma_trace:
            return {"phase": "UNKNOWN", "critical": False}
        current = float(self.sigma_trace[-1])
        ta = float(getattr(self.gate, "threshold_accept", 0.0))
        tb = float(getattr(self.gate, "threshold_abstain", 1.0))
        return {
            "current_sigma": round(current, 4),
            "phase": self.phase(current),
            "critical": self.is_critical(),
            "distance": self.distance_from_criticality(current),
            "nudge": self.nudge_toward_critical(current),
            "gate_threshold_accept": ta,
            "gate_threshold_abstain": tb,
        }
