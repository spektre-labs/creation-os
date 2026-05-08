# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Social / ToM lab: trust from σ-history, intent prediction, meta–theory-of-mind gaps.

**Not** clinical or forensic person modeling — templates and σ only. See ``docs/CLAIM_DISCIPLINE.md``.

:class:`SigmaSocialV2` and :class:`AgentModel` add recursive ToM **pedagogy** (gate-scored only)."""
from __future__ import annotations

from typing import Any, Dict, List, Mapping, Sequence

from cos.sigma_gate import SigmaGate

__all__ = ["AgentModel", "SigmaSocial", "SigmaSocialV2"]


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


class AgentModel:
    """Lightweight model of another agent's observed behavior (lab)."""

    def __init__(self, agent_id: str, gate: Any) -> None:
        self.agent_id = str(agent_id)
        self.gate = gate
        self.believed_goals: List[Any] = []
        self.believed_σ = 0.5
        self.observations: List[Any] = []
        self.prediction_errors: List[float] = []

    def observe(self, behavior: Any) -> None:
        self.observations.append(behavior)
        if len(self.observations) >= 2:
            σ, _ = self.gate.score(
                str(self.observations[-2]),
                str(self.observations[-1]),
            )
            sg = float(σ)
            self.prediction_errors.append(sg)
            self.believed_σ = sg

    def predict(self, context: str) -> Dict[str, Any]:
        last = str(self.observations[-1]) if self.observations else "unknown"
        σ, _ = self.gate.score(
            f"agent {self.agent_id} in {context}",
            last,
        )
        sg = float(σ)
        return {
            "agent": self.agent_id,
            "predicted_σ": round(sg, 4),
            "confidence": round(1.0 - sg, 4),
        }


class SigmaSocialV2:
    """Recursive ToM hooks scored only via :class:`~cos.sigma_gate.SigmaGate` (**NOT** AGI / clinical)."""

    def __init__(self, my_id: str = "self", gate: Any = None) -> None:
        self.my_id = str(my_id)
        self.gate = gate or SigmaGate()
        self.agent_models_v2: Dict[str, AgentModel] = {}
        self.tom_depth = 0

    def model_agent(self, agent_id: str) -> AgentModel:
        aid = str(agent_id)
        if aid not in self.agent_models_v2:
            self.agent_models_v2[aid] = AgentModel(aid, self.gate)
        return self.agent_models_v2[aid]

    def observe_agent(self, agent_id: str, behavior: Any) -> None:
        self.model_agent(agent_id).observe(behavior)

    def level_0(self, my_state: Any) -> Dict[str, Any]:
        σ, verdict = self.gate.score("my belief", str(my_state))
        return {
            "level": 0,
            "σ": round(float(σ), 4),
            "verdict": _verdict_str(verdict),
            "description": "act on own beliefs",
        }

    def level_1(self, agent_id: str, context: str = "") -> Dict[str, Any]:
        self.tom_depth = max(self.tom_depth, 1)
        model = self.model_agent(agent_id)
        prediction = model.predict(context)
        return {
            "level": 1,
            "agent": agent_id,
            "σ": prediction["predicted_σ"],
            "confidence": prediction["confidence"],
            "description": f"I predict {agent_id}'s behavior",
            "n_observations": len(model.observations),
        }

    def level_2(self, agent_id: str, context: str = "") -> Dict[str, Any]:
        self.tom_depth = max(self.tom_depth, 2)
        σ_their_model, _ = self.gate.score(
            f"{agent_id} thinks I will",
            f"my actual behavior in {context}",
        )
        σ_meta, _ = self.gate.score(
            f"my model of {agent_id}'s model of me",
            f"what {agent_id} actually thinks",
        )
        st = float(σ_their_model)
        sm = float(σ_meta)
        if sm < 0.3:
            mu = "high"
        elif sm < 0.6:
            mu = "moderate"
        else:
            mu = "low"
        return {
            "level": 2,
            "agent": agent_id,
            "σ_their_model_of_me": round(st, 4),
            "σ_meta": round(sm, 4),
            "description": f"I model {agent_id}'s model of me",
            "mutual_understanding": mu,
        }

    def meta_tom(
        self,
        my_σ: float,
        their_σ_of_me: float,
        my_σ_of_their_σ: float,
    ) -> Dict[str, Any]:
        m = float(my_σ)
        t = float(their_σ_of_me)
        m2 = float(my_σ_of_their_σ)
        total_gap = abs(m - t) + abs(t - m2)
        if total_gap < 0.2:
            mu = "high"
        elif total_gap < 0.5:
            mu = "moderate"
        else:
            mu = "low"
        return {
            "my_σ": round(m, 4),
            "their_σ_of_me": round(t, 4),
            "my_σ_of_their_σ": round(m2, 4),
            "total_gap": round(total_gap, 4),
            "mutual_understanding": mu,
            "asymmetry": round(abs(m - t), 4),
            "insight": (
                "low cross-agent σ gap — aligned mutual models (lab)"
                if total_gap < 0.1
                else "σ gap between modeled layers — refine models (lab)"
            ),
        }

    def belief_resonance(
        self,
        agent_id: str,
        my_goal: Any,
        their_inferred_goal: Any,
    ) -> Dict[str, Any]:
        σ_before, _ = self.gate.score("my goal", str(my_goal))
        σ_after, _ = self.gate.score(
            f"my goal considering {agent_id}'s goal: {their_inferred_goal}",
            str(my_goal),
        )
        sb = float(σ_before)
        sa = float(σ_after)
        resonance = abs(sb - sa)
        return {
            "agent": agent_id,
            "σ_before_resonance": round(sb, 4),
            "σ_after_resonance": round(sa, 4),
            "resonance_strength": round(resonance, 4),
            "influenced": resonance > 0.1,
            "description": f"Modeling {agent_id} shifted my σ by ~{resonance:.3f} (lab)",
        }

    def cooperation_potential(self, agents: Sequence[str]) -> Dict[str, Any]:
        agents_list = [str(a) for a in agents]
        pairs: List[Dict[str, Any]] = []
        for i, a in enumerate(agents_list):
            for b in agents_list[i + 1 :]:
                σ, _ = self.gate.score(
                    f"agent {a} cooperates with",
                    f"agent {b}",
                )
                sg = float(σ)
                pairs.append(
                    {
                        "pair": (a, b),
                        "σ": round(sg, 4),
                        "can_cooperate": sg < 0.4,
                    }
                )
        avg_σ = sum(p["σ"] for p in pairs) / max(len(pairs), 1)
        return {
            "pairs": pairs,
            "avg_cooperation_σ": round(float(avg_σ), 4),
            "team_viable": float(avg_σ) < 0.4,
        }
