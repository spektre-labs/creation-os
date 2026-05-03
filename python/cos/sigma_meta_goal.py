# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
v182 σ-meta-goal: goal proposals from metacog competence + optional σ homeostasis (lab).

**Not autonomous agency in the wild:** ``act_on_goal`` returns an instrumented audit plan only;
wire tools and policy before any unsupervised loop.
"""
from __future__ import annotations

from typing import Any, Dict, List


class SigmaMetaGoal:
    def __init__(self, gate: Any, metacog: Any, drives: Any, alignment: Any) -> None:
        self.gate = gate
        self.metacog = metacog
        self.drives = drives
        self.alignment = alignment
        self._view = gate if hasattr(gate, "avg_sigma") else None
        if self._view is None:
            try:
                from cos.sigma_drive import DriveGateView

                self._view = DriveGateView(gate)
            except Exception:
                self._view = gate

    def _avg_sigma(self) -> float:
        fn = getattr(self._view, "avg_sigma", None)
        if callable(fn):
            try:
                return float(fn())
            except Exception:
                return 0.5
        return 0.5

    def generate_goals(self) -> List[Dict[str, Any]]:
        goals: List[Dict[str, Any]] = []
        knowledge: Dict[str, Any] = {}
        if callable(getattr(self.metacog, "knowledge_inventory", None)):
            try:
                knowledge = self.metacog.knowledge_inventory()
            except Exception:
                knowledge = {}

        weak_domains = [
            d
            for d, s in knowledge.items()
            if isinstance(s, dict) and float(s.get("competence", 0.5)) < 0.3 and int(s.get("queries", 0)) > 3
        ]
        for domain in weak_domains[:3]:
            comp = float(knowledge[domain].get("competence", 0.0)) if isinstance(knowledge.get(domain), dict) else 0.0
            goals.append(
                {
                    "type": "curiosity",
                    "goal": f"Improve competence in {domain}",
                    "priority": max(0.0, min(1.0, 1.0 - comp)),
                    "source": "knowledge_gap",
                }
            )

        strong_domains = [
            d
            for d, s in knowledge.items()
            if isinstance(s, dict) and 0.6 < float(s.get("competence", 0.0)) < 0.9
        ]
        for domain in strong_domains[:2]:
            goals.append(
                {
                    "type": "competence",
                    "goal": f"Solidify mastery in {domain}",
                    "priority": 0.7,
                    "source": "near_mastery",
                }
            )

        avg_sigma = self._avg_sigma()
        if avg_sigma > 0.3:
            goals.append(
                {
                    "type": "homeostasis",
                    "goal": "Reduce average σ via calibration or additional context",
                    "priority": max(0.0, min(1.0, avg_sigma)),
                    "source": "sigma_drift",
                }
            )

        filtered: List[Dict[str, Any]] = []
        for g in goals:
            goal_text = str(g.get("goal", ""))
            if callable(getattr(self.alignment, "check_alignment", None)):
                try:
                    chk = self.alignment.check_alignment("goal_proposal", goal_text)
                except Exception:
                    chk = {"aligned": True}
            else:
                chk = {"aligned": True}
            if chk.get("aligned", True):
                g2 = dict(g)
                g2["alignment_check"] = chk
                filtered.append(g2)

        filtered.sort(key=lambda x: float(x.get("priority", 0.0)), reverse=True)
        return filtered

    def learning_progress(self) -> Dict[str, Any]:
        """Aggregate competence stats from metacog inventory when available."""
        knowledge: Dict[str, Any] = {}
        if callable(getattr(self.metacog, "knowledge_inventory", None)):
            try:
                knowledge = self.metacog.knowledge_inventory()
            except Exception:
                knowledge = {}
        comps = [
            float(s.get("competence", 0.0))
            for s in knowledge.values()
            if isinstance(s, dict) and "competence" in s
        ]
        if not comps:
            return {
                "domains_tracked": 0,
                "avg_competence": 0.0,
                "weak_domains": 0,
                "note": "empty metacog inventory",
            }
        weak = sum(1 for c in comps if c < 0.3)
        return {
            "domains_tracked": len(comps),
            "avg_competence": round(sum(comps) / len(comps), 4),
            "weak_domains": int(weak),
        }

    def curriculum(self, max_items: int = 5) -> List[Dict[str, Any]]:
        """Ordered study plan: reuse goal priorities weakest-first."""
        goals = self.generate_goals()
        return list(goals[: max(0, int(max_items))])

    def act_on_goal(self, goal: Dict[str, Any]) -> Dict[str, Any]:
        plan: Dict[str, Any] = {
            "kind": "instrumented_plan",
            "goal_type": goal.get("type"),
            "steps": [
                "gather_evidence_and_context",
                "execute_one_bounded_action",
                "sigma_verify_before_commit",
            ],
        }
        if callable(getattr(self.alignment, "check_alignment", None)):
            try:
                plan["alignment"] = self.alignment.check_alignment("action_plan", str(goal.get("goal", "")))
            except Exception:
                plan["alignment"] = {"aligned": True, "note": "alignment_check_failed"}
        else:
            plan["alignment"] = {"aligned": True}
        plan[
            "note"
        ] = "Closed-loop autonomy requires wired tools; this returns an audit plan only."
        return plan

    def autonomous_step(self) -> Dict[str, Any]:
        goals = self.generate_goals()
        if not goals:
            return {"action": "idle", "reason": "no goals generated"}
        top_goal = goals[0]
        action = self.act_on_goal(top_goal)
        reflection: Dict[str, Any] = {}
        if callable(getattr(self.metacog, "introspect", None)):
            try:
                reflection = self.metacog.introspect(str(top_goal.get("goal", "")))
            except Exception:
                reflection = {"error": "introspect_failed"}
        return {"goal": top_goal, "action": action, "reflection": reflection}


__all__ = ["SigmaMetaGoal"]
