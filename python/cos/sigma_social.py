# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
v171 σ-social: coarse Theory-of-Mind style sketches + σ-scored predictions (lab).

**Not person modeling:** text templates only; trust ledger is a toy EMA.
"""
from __future__ import annotations

from typing import Any, Dict, List, Mapping


class SigmaSocial:
    def __init__(self, gate: Any, model: Any) -> None:
        self.gate = gate
        self.model = model
        self.agent_models: Dict[str, Dict[str, Any]] = {}
        self.trust_ledger: Dict[str, float] = {}
        self._engram_scratch: List[str] = []

    def engram_store(self, line: str) -> None:
        self._engram_scratch.append(str(line))

    def model_other(self, agent_id: str, observations: str) -> Dict[str, Any]:
        mental_model = {
            "beliefs": self.infer_beliefs(agent_id, observations),
            "intentions": self.infer_intentions(agent_id, observations),
            "emotional_state": self.infer_emotions(agent_id, observations),
            "reliability": float(self.trust_ledger.get(agent_id, 0.5)),
        }
        self.agent_models[agent_id] = mental_model
        return mental_model

    def predict_behavior(self, agent_id: str, situation: str) -> Dict[str, Any]:
        model = self.agent_models.get(agent_id)
        if not model:
            return {"prediction": None, "sigma": 1.0}
        prompt = (
            f"Agent {agent_id} believes: {model['beliefs']}\n"
            f"Agent intends: {model['intentions']}\n"
            f"Situation: {situation}\n"
            f"What will the agent do?"
        )
        prediction = self.model.generate(prompt)
        sigma, verdict = self.gate.score(prompt, prediction)
        return {"prediction": prediction, "sigma": float(sigma), "verdict": verdict}

    def learn_from_other(self, agent_id: str, outcome: Mapping[str, Any]) -> None:
        success = bool(outcome.get("success"))
        strat = str(outcome.get("strategy", ""))
        if success:
            self.engram_store(f"Learned from {agent_id}: {strat} works")
            self.update_trust(agent_id, 0.05)
        else:
            self.engram_store(f"Learned from {agent_id}: {strat} fails")
            self.update_trust(agent_id, -0.02)

    def update_trust(self, agent_id: str, delta: float) -> None:
        current = float(self.trust_ledger.get(agent_id, 0.5))
        self.trust_ledger[agent_id] = max(0.0, min(1.0, current + float(delta)))

    def infer_beliefs(self, agent_id: str, observations: str) -> str:
        return self.model.generate(f"Based on {observations}, what does agent {agent_id} believe?")

    def infer_intentions(self, agent_id: str, observations: str) -> str:
        return self.model.generate(f"Based on {observations}, what does agent {agent_id} intend to do?")

    def infer_emotions(self, agent_id: str, observations: str) -> str:
        return self.model.generate(f"Based on {observations}, how does agent {agent_id} feel?")

    def model_other_agent(self, agent_id: str, observations: str) -> Dict[str, Any]:
        """Readable alias for :meth:`model_other`."""
        return self.model_other(agent_id, observations)

    def predict_intent(self, agent_id: str, situation: str) -> Dict[str, Any]:
        intent_text = self.infer_intentions(agent_id, situation)
        sigma, verdict = self.gate.score(f"situation:{situation[:500]}", intent_text)
        return {"intent": intent_text, "sigma": float(sigma), "verdict": str(verdict)}

    def meta_tom(self, observer_id: str, target_id: str, situation: str) -> Dict[str, Any]:
        """Second-order ToM sketch: what observer thinks target believes."""
        target_beliefs = self.infer_beliefs(target_id, situation)
        prompt = (
            f"Observer {observer_id}: what does agent {target_id} believe about the situation?\n"
            f"Evidence / model: {target_beliefs}\nSituation: {situation[:600]}"
        )
        hypo = self.model.generate(prompt)
        sigma, verdict = self.gate.score(prompt, hypo)
        return {"hypothesis": hypo, "sigma": float(sigma), "verdict": str(verdict)}


__all__ = ["SigmaSocial"]
