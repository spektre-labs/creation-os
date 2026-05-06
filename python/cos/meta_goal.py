# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Autonomous goal generation via σ-gradient curiosity (IMGEP-style).

The agent proposes its own practice targets from σ trajectories per skill:
learning progress is estimated as Δσ/Δt (slope of recent σ); σ is the only
optimization signal — no external reward. See ``docs/CLAIM_DISCIPLINE.md``."""

from __future__ import annotations

from typing import Any, Dict, List, Optional

from cos.sigma_gate import SigmaGate


class SigmaMetaGoal:
    """Intrinsically motivated goal generation from σ dynamics alone."""

    def __init__(self, gate: Optional[SigmaGate] = None) -> None:
        self.gate = gate or SigmaGate()
        self.skills: Dict[str, List[float]] = {}
        self.goals: List[Dict[str, Any]] = []
        self.curriculum: List[Dict[str, Any]] = []

    def register_skill(self, name: str) -> None:
        if name not in self.skills:
            self.skills[name] = []

    def record(self, skill_name: str, sigma: float) -> None:
        """Append one σ observation for a skill attempt."""
        self.register_skill(skill_name)
        self.skills[skill_name].append(float(sigma))

    def learning_progress(self, skill_name: str, window: int = 5) -> float:
        """Δσ/Δt proxy: negated OLS slope over the last ``window`` samples.

        Positive ⇒ σ tends to decrease (improving coherence on that skill).
        """
        history = self.skills.get(skill_name, [])
        if len(history) < 2:
            return 0.0
        w = max(1, int(window))
        recent = history[-w:]
        if len(recent) < 2:
            return 0.0
        n = len(recent)
        x_mean = (n - 1) / 2.0
        y_mean = sum(recent) / n
        numerator = sum((i - x_mean) * (y - y_mean) for i, y in enumerate(recent))
        denominator = sum((i - x_mean) ** 2 for i in range(n))
        if denominator == 0.0:
            return 0.0
        slope = numerator / denominator
        return round(float(-slope), 6)

    def current_sigma(self, skill_name: str) -> float:
        history = self.skills.get(skill_name, [])
        return float(history[-1]) if history else 1.0

    # Alias for API symmetry with σ notation in docs / JSON
    def current_σ(self, skill_name: str) -> float:
        return self.current_sigma(skill_name)

    def generate_goals(self, top_n: int = 5) -> List[Dict[str, Any]]:
        """Rank skills by intrinsic interest (learning progress), capped at ``top_n``."""
        scored: List[Dict[str, Any]] = []
        for skill_name in self.skills:
            lp = self.learning_progress(skill_name)
            current = self.current_sigma(skill_name)
            if current < 0.1:
                interest = 0.0
            elif current > 0.95 and lp <= 0.0:
                interest = -1.0
            else:
                interest = lp
            scored.append(
                {
                    "skill": skill_name,
                    "learning_progress": lp,
                    "current_σ": round(current, 6),
                    "interest": round(float(interest), 4),
                }
            )
        scored.sort(key=lambda x: x["interest"], reverse=True)
        self.goals = scored[: max(0, int(top_n))]
        return self.goals

    def build_curriculum(self) -> List[Dict[str, Any]]:
        """Ordered path: active learners only (positive interest); steepest progress first."""
        goals = self.generate_goals(top_n=len(self.skills))
        self.curriculum = [g for g in goals if float(g["interest"]) > 0.0]
        self.curriculum.sort(key=lambda x: x["interest"], reverse=True)
        return self.curriculum

    def next_goal(self) -> Optional[Dict[str, Any]]:
        if not self.curriculum:
            self.build_curriculum()
        if self.curriculum:
            return self.curriculum[0]
        least = min(self.skills.items(), key=lambda kv: len(kv[1]), default=None)
        if least:
            return {
                "skill": least[0],
                "interest": 0.0,
                "learning_progress": self.learning_progress(least[0]),
                "current_σ": self.current_sigma(least[0]),
                "reason": "least practiced",
            }
        return None
