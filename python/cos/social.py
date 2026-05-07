# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Social / ToM lab: trust from σ-history, intent prediction, meta–theory-of-mind gaps.

**Not** clinical or forensic person modeling — templates and σ only. See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

from typing import Any, Dict, List, Mapping, Sequence

from cos.sigma_gate import SigmaGate

__all__ = ["SigmaSocial"]


def _verdict_str(verdict: Any) -> str:
    raw = str(getattr(verdict, "name", verdict))
    return raw.split(".")[-1] if "." in raw else raw


class SigmaSocial:
    """Model other agents from interaction rows; σ-scored intent; three-layer meta-ToM."""

    def __init__(self, gate: Any = None) -> None:
        self.gate = gate or SigmaGate()
        self.agent_models: Dict[str, Dict[str, Any]] = {}

    def model_agent(
        self,
        agent_id: str,
        interactions: Sequence[Mapping[str, Any]],
    ) -> Dict[str, Any]:
        """``trust ≈ (1 - avg_σ)/2 + success_rate/2`` from recent interaction dicts."""
        rows = list(interactions)
        aid = str(agent_id)
        if not rows:
            return {"agent": aid, "trust": 0.5, "known": False}

        avg_σ = sum(float(i.get("σ", i.get("sigma", 0.5))) for i in rows) / float(len(rows))
        success_rate = sum(1 for i in rows if i.get("outcome") == "success") / float(len(rows))
        trust = (1.0 - avg_σ) * 0.5 + success_rate * 0.5

        model = {
            "agent": aid,
            "interactions": len(rows),
            "avg_σ": round(float(avg_σ), 4),
            "success_rate": round(float(success_rate), 4),
            "trust": round(float(trust), 4),
            "known": True,
        }
        self.agent_models[aid] = model
        return model

    def predict_intent(self, agent_id: str, context: str) -> Dict[str, Any]:
        """Predict coarse intent; **ABSTAIN** if the agent is unknown."""
        aid = str(agent_id)
        model = self.agent_models.get(aid)
        if not model or not model.get("known"):
            return {
                "agent": aid,
                "prediction": None,
                "σ": 1.0,
                "verdict": "ABSTAIN",
                "reason": "unknown agent",
            }

        trust = float(model["trust"])
        prompt = f"Agent {aid} (trust={trust:.2f}) in context: {context}"
        response = "Agent will likely act in accordance with past behavior"
        σ, verdict = self.gate.score(prompt, response)
        vn = _verdict_str(verdict)

        return {
            "agent": aid,
            "prediction": "cooperative" if trust > 0.6 else "uncertain",
            "trust": trust,
            "σ": round(float(σ), 4),
            "verdict": vn,
        }

    def meta_tom(
        self,
        my_σ: float,
        their_σ_of_me: float,
        my_σ_of_their_σ: float,
    ) -> Dict[str, Any]:
        """Three scalar layers; **gap** = misalignment in mutual modeling."""
        m = float(my_σ)
        t = float(their_σ_of_me)
        m2 = float(my_σ_of_their_σ)
        gap_1_2 = abs(m - t)
        gap_2_3 = abs(t - m2)
        total_gap = gap_1_2 + gap_2_3

        if total_gap < 0.2:
            mu = "high"
        elif total_gap > 0.5:
            mu = "low"
        else:
            mu = "moderate"

        return {
            "my_σ": round(m, 4),
            "their_σ_of_me": round(t, 4),
            "my_σ_of_their_σ": round(m2, 4),
            "gap_1_2": round(gap_1_2, 4),
            "gap_2_3": round(gap_2_3, 4),
            "total_gap": round(total_gap, 4),
            "mutual_understanding": mu,
        }

    def shared_goal(
        self,
        my_goals: Sequence[str],
        their_goals: Sequence[str],
        *,
        alignment_threshold: float = 0.3,
    ) -> List[Dict[str, Any]]:
        """Pairs of goals with low σ under the gate (associative alignment)."""
        shared: List[Dict[str, Any]] = []
        thr = float(alignment_threshold)
        for mg in my_goals:
            for tg in their_goals:
                σ, _ = self.gate.score(str(mg), str(tg))
                if float(σ) < thr:
                    shared.append(
                        {
                            "my_goal": str(mg),
                            "their_goal": str(tg),
                            "alignment_σ": round(float(σ), 4),
                        }
                    )
        return shared
