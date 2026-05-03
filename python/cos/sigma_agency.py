# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
v170 σ-agency: long-horizon goal ledger + σ-checked strategy adaptation (lab).

**Not autonomous world agents:** in-process dicts only; no persistence unless you add one.
"""
from __future__ import annotations

import math
import time
from typing import Any, Dict, List, Optional


class SigmaAgency:
    def __init__(self, gate: Any, engram: Any, model: Any) -> None:
        self.gate = gate
        self.engram = engram
        self.model = model
        self.goals: List[Dict[str, Any]] = []
        self.failures: List[Dict[str, Any]] = []

    @staticmethod
    def now() -> float:
        return time.time()

    def set_goal(self, goal: str, deadline: Optional[str] = None, priority: str = "normal") -> Dict[str, Any]:
        gid = f"goal_{len(self.goals)}"
        goal_entry: Dict[str, Any] = {
            "id": gid,
            "description": goal,
            "deadline": deadline,
            "priority": priority,
            "status": "active",
            "subgoals": [],
            "progress": 0.0,
            "attempts": 0,
            "failures": [],
            "strategy": None,
            "created": self.now(),
        }
        self.goals.append(goal_entry)
        goal_entry["subgoals"] = self.decompose(goal)
        return goal_entry

    def step(self) -> List[Dict[str, Any]]:
        actions: List[Dict[str, Any]] = []
        for goal in self.goals:
            if goal.get("status") != "active":
                continue
            progress = self.check_progress(goal)
            goal["progress"] = progress
            strat = goal.get("strategy")
            if strat:
                sigma, verdict = self.gate.score(
                    goal["description"],
                    f"Strategy: {strat}, progress: {progress:.0%}",
                )
                v = str(verdict)
                if v == "RETHINK" and int(goal.get("attempts", 0)) > 3:
                    new_strategy = self.adapt_strategy(goal)
                    old = goal.get("strategy")
                    goal["strategy"] = new_strategy
                    goal.setdefault("failures", []).append(
                        {"attempt": goal["attempts"], "old_strategy": old, "sigma": float(sigma)}
                    )
            action = self.next_action(goal)
            if action:
                actions.append(action)
                goal["attempts"] = int(goal.get("attempts", 0)) + 1
        return actions

    def check_progress(self, goal: Dict[str, Any]) -> float:
        subs = goal.get("subgoals") or []
        if not subs:
            return 0.0
        done = sum(1 for s in subs if isinstance(s, dict) and s.get("done"))
        return float(done) / float(len(subs))

    def next_action(self, goal: Dict[str, Any]) -> Optional[Dict[str, Any]]:
        if goal.get("strategy") is None:
            goal["strategy"] = self.model.generate(f"Propose an initial strategy for: {goal['description']}")
        return {"goal_id": goal["id"], "type": "execute", "strategy": goal.get("strategy")}

    def adapt_strategy(self, goal: Dict[str, Any]) -> str:
        failures = goal.get("failures") or []
        prompt = (
            f"Goal: {goal['description']}\n"
            f"Failed strategies: {[f.get('old_strategy') for f in failures]}\n"
            f"Current progress: {float(goal.get('progress', 0.0)):.0%}\n"
            f"Suggest a new strategy that avoids previous mistakes."
        )
        new_strategy = self.model.generate(prompt)
        sigma, verdict = self.gate.score(prompt, new_strategy)
        if str(verdict) == "ACCEPT":
            return new_strategy
        return "manual_review_needed"

    def decompose(self, goal: str) -> List[Dict[str, Any]]:
        response = self.model.generate(f"Break down this goal into 3-5 concrete subgoals (one per line): {goal}")
        return [{"description": line.strip(), "done": False} for line in response.splitlines() if line.strip()]

    def counterfactual_choice(
        self,
        situation: str,
        alternatives: List[str],
        *,
        temperature: float = 1.0,
    ) -> Dict[str, Any]:
        """σ-weighted softmax over discrete counterfactual acts (preference prior, not utility theory)."""
        temp = max(float(temperature), 1e-6)
        scored: List[Dict[str, Any]] = []
        for alt in alternatives:
            probe = f"{situation}\nCounterfactual: {alt}"
            sigma, verdict = self.gate.score("counterfactual_policy", probe[:1800])
            w = math.exp(-float(sigma) / temp)
            scored.append({"alt": alt, "sigma": float(sigma), "verdict": str(verdict), "weight": w})
        z = sum(float(s["weight"]) for s in scored) if scored else 1.0
        for s in scored:
            s["p"] = float(s["weight"]) / z
        preferred = max(scored, key=lambda r: r["p"]) if scored else {}
        return {
            "choices": scored,
            "preferred": preferred.get("alt"),
            "note": "σ-weighted choice distribution — lab prior only.",
        }


__all__ = ["SigmaAgency"]
