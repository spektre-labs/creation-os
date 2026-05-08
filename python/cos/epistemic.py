# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Epistemic agency lab (σ-driven): high-uncertainty outputs trigger **planned** follow-up, not silence.

The σ-gate labels **ACCEPT / RETHINK / ABSTAIN** on ``(prompt, response)`` pairs. This module uses
those scores **as a toy policy signal** to choose *assert*, *hedge*, or *explore* — a lab hook for
“unknown → plan”, not a proof of calibrated epistemics or **AGI** (see ``docs/CLAIM_DISCIPLINE.md``).

``sigma_gate.h`` is **not** modified by this module.
"""
from __future__ import annotations

from typing import Any, Dict, List, Optional

from cos.sigma_gate import SigmaGate

__all__ = ["EpistemicAgent"]


def _verdict_label(verdict: Any) -> str:
    raw = str(getattr(verdict, "name", verdict))
    base = raw.split(".")[-1] if "." in raw else raw
    return base.upper()


def _mode_from_sigma(sigma: float) -> tuple[str, str]:
    if sigma < 0.2:
        return "ASSERT", "state confidently"
    if sigma < 0.5:
        return "HEDGE", "state with uncertainty markers"
    return "EXPLORE", "generate exploration plan"


class EpistemicAgent:
    """Lab agent: map topic-level knowledge claims to σ modes and optional exploration plans."""

    def __init__(self, gate: Optional[Any] = None) -> None:
        self.gate = gate if gate is not None else SigmaGate()
        self.knowledge_map: Dict[str, Dict[str, Any]] = {}
        self.explorations: List[Dict[str, Any]] = []

    def assess(self, topic: str, current_knowledge: str = "") -> Dict[str, Any]:
        """Score a self-query about ``topic``; store σ mode (ASSERT / HEDGE / EXPLORE)."""
        prompt = f"what do I know about {topic}"
        response = current_knowledge or "uncertain"
        sigma, verdict = self.gate.score(str(prompt), str(response))
        sigma = float(sigma)
        mode, action = _mode_from_sigma(sigma)

        assessment: Dict[str, Any] = {
            "topic": topic,
            "σ": round(sigma, 4),
            "mode": mode,
            "action": action,
            "verdict": _verdict_label(verdict),
        }
        self.knowledge_map[str(topic)] = assessment
        return assessment

    def explore_plan(self, topic: str) -> Dict[str, Any]:
        """If ``topic`` is in EXPLORE mode, propose σ-scored steps; otherwise return ``plan: None``."""
        assessment = self.knowledge_map.get(str(topic))
        if not assessment or assessment.get("mode") != "EXPLORE":
            return {
                "plan": None,
                "reason": "topic is known or not assessed",
            }

        strategies: List[Dict[str, Any]] = [
            {
                "strategy": "search",
                "description": f"search for {topic}",
                "expected_σ_reduction": 0.3,
            },
            {
                "strategy": "decompose",
                "description": f"break {topic} into sub-questions",
                "expected_σ_reduction": 0.2,
            },
            {
                "strategy": "analogize",
                "description": f"find known domain similar to {topic}",
                "expected_σ_reduction": 0.15,
            },
            {
                "strategy": "experiment",
                "description": f"test hypothesis about {topic}",
                "expected_σ_reduction": 0.4,
            },
            {
                "strategy": "ask_expert",
                "description": f"escalate {topic} to human expert",
                "expected_σ_reduction": 0.5,
            },
        ]

        for s in strategies:
            s_prompt = f"will {s['strategy']} help with {topic}?"
            sig, _ = self.gate.score(s_prompt, str(s["description"]))
            s["σ_feasibility"] = round(float(sig), 4)

        strategies.sort(key=lambda s: float(s["σ_feasibility"]))

        plan = {
            "topic": str(topic),
            "current_σ": assessment["σ"],
            "target_σ": 0.2,
            "steps": strategies[:3],
            "estimated_steps_to_knowledge": self._estimate_steps(float(assessment["σ"])),
        }

        self.explorations.append(plan)
        return plan

    def execute_exploration(
        self,
        topic: str,
        result: Any,
        strategy_used: str,
    ) -> Dict[str, Any]:
        """Re-score knowledge after an exploration step; compare σ to the prior assessment."""
        sigma_before = float(self.knowledge_map.get(str(topic), {}).get("σ", 1.0))
        prompt = f"after {strategy_used}: what do I know about {topic}"
        sigma_after, verdict = self.gate.score(prompt, str(result))
        sigma_after = float(sigma_after)
        improvement = sigma_before - sigma_after
        if sigma_after < 0.2:
            mode = "ASSERT"
        elif sigma_after < 0.5:
            mode = "HEDGE"
        else:
            mode = "EXPLORE"

        self.knowledge_map[str(topic)] = {
            "topic": str(topic),
            "σ": round(sigma_after, 4),
            "mode": mode,
            "action": "learned" if improvement > 0 else "still exploring",
            "verdict": _verdict_label(verdict),
        }

        return {
            "topic": str(topic),
            "σ_before": round(sigma_before, 4),
            "σ_after": round(sigma_after, 4),
            "improvement": round(improvement, 4),
            "learned": improvement > 0.1,
            "still_unknown": sigma_after > 0.5,
            "strategy": str(strategy_used),
        }

    def knowledge_frontier(self) -> Dict[str, Any]:
        """Partition assessed topics by current σ mode."""
        assert_topics: List[Dict[str, Any]] = []
        hedge_topics: List[Dict[str, Any]] = []
        explore_topics: List[Dict[str, Any]] = []

        for topic, info in self.knowledge_map.items():
            mode = info.get("mode")
            if mode == "ASSERT":
                assert_topics.append({"topic": topic, "σ": info["σ"]})
            elif mode == "HEDGE":
                hedge_topics.append({"topic": topic, "σ": info["σ"]})
            else:
                explore_topics.append({"topic": topic, "σ": info["σ"]})

        total = len(self.knowledge_map)
        return {
            "known": assert_topics,
            "partially_known": hedge_topics,
            "unknown": explore_topics,
            "total_topics": total,
            "knowledge_ratio": round(len(assert_topics) / float(max(total, 1)), 4),
            "frontier_size": len(explore_topics),
        }

    def _estimate_steps(
        self,
        current_sigma: float,
        target_sigma: float = 0.2,
        avg_reduction: float = 0.15,
    ) -> int:
        """Heuristic step count from σ gap and an assumed average reduction per step (lab only)."""
        if current_sigma <= target_sigma:
            return 0
        gap = float(current_sigma) - float(target_sigma)
        return max(1, int(gap / float(avg_reduction)) + 1)

    def curiosity_signal(self) -> Dict[str, Any]:
        """Pick the EXPLORE topic with the highest σ (lab curiosity proxy)."""
        unknowns = [
            (t, float(info["σ"]))
            for t, info in self.knowledge_map.items()
            if info.get("mode") == "EXPLORE"
        ]
        unknowns.sort(key=lambda x: x[1], reverse=True)
        if unknowns:
            return {
                "most_curious": unknowns[0][0],
                "σ": unknowns[0][1],
                "reason": "highest σ = most unknown = most worth exploring",
            }
        return {"most_curious": None, "reason": "nothing unknown"}
