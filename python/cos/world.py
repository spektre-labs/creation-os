# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

"""σ-world — lightweight world-model lab (commonsense check, action prediction, rollout).

Uses an optional :class:`~cos.graph.SigmaGraph` for **heuristic** grounding. This is **not**
a physics engine or AGI world model. See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import time
from typing import Any, Dict, List, Optional, Sequence

from cos.sigma_gate import SigmaGate

__all__ = ["SigmaWorld", "WorldState"]


def _verdict_str(verdict: Any) -> str:
    raw = str(getattr(verdict, "name", verdict))
    return raw.split(".")[-1] if "." in raw else raw


class WorldState:
    """One recorded observation in the running state list."""

    def __init__(
        self,
        description: str,
        features: Optional[Dict[str, Any]] = None,
        timestamp: Optional[float] = None,
    ) -> None:
        self.description = str(description)
        self.features = dict(features or {})
        self.timestamp = float(timestamp if timestamp is not None else time.time())

    def __repr__(self) -> str:
        return f"State({self.description[:40]!r})"


class SigmaWorld:
    """Observe → forecast → one-step transition (gate); optional graph for commonsense."""

    def __init__(
        self,
        gate: Any = None,
        max_history: int = 100,
        *,
        graph: Any = None,
    ) -> None:
        self.gate = gate or SigmaGate()
        self.graph = graph
        self.states: List[WorldState] = []
        self.max_history = int(max_history)
        self.predictions: List[Dict[str, Any]] = []

    def observe(
        self,
        description: str,
        features: Optional[Dict[str, Any]] = None,
    ) -> WorldState:
        state = WorldState(description, features)
        self.states.append(state)
        if len(self.states) > self.max_history:
            self.states = self.states[-self.max_history :]
        self._validate_predictions(description)
        return state

    def forecast(self, query: str, n_steps: int = 1) -> Dict[str, Any]:
        """Heuristic forecast from recent state text + feature drift (legacy path)."""
        del n_steps
        if not self.states:
            return {"prediction": None, "confidence": 0.0, "basis": "no history", "query": query}

        recent = self.states[-10:]
        feature_trend = self._analyze_feature_trend()

        prediction: Dict[str, Any] = {
            "query": query,
            "basis_states": len(recent),
            "feature_trend": feature_trend,
            "timestamp": time.time(),
        }
        self.predictions.append(prediction)
        return prediction

    def transition(self, action: str, current_state: Optional[WorldState] = None) -> Dict[str, Any]:
        """Score a single action against the latest (or given) :class:`WorldState`."""
        if current_state is None:
            current_state = self.states[-1] if self.states else None

        if current_state is None:
            return {"outcome": None, "sigma": 1.0, "verdict": "ABSTAIN"}

        scenario = f"Given state: {current_state.description}. Action: {action}"
        sigma, verdict = self.gate.score(scenario, action)
        return {
            "action": action,
            "current_state": current_state.description,
            "sigma": round(float(sigma), 4),
            "verdict": str(verdict),
            "n_states_considered": len(self.states),
        }

    def plan(self, goal: str, max_steps: int = 5) -> Dict[str, Any]:
        steps: List[Dict[str, Any]] = []
        for i in range(max_steps):
            step_desc = f"Step {i + 1} toward: {goal}"
            sim = self.transition(step_desc)
            steps.append(
                {
                    "step": i + 1,
                    "action": step_desc,
                    "sigma": sim["sigma"],
                    "verdict": sim["verdict"],
                }
            )
            if sim["verdict"] == "ACCEPT":
                break
            if sim["verdict"] == "ABSTAIN":
                break

        total_sigma = sum(float(s["sigma"]) for s in steps) / max(len(steps), 1)
        return {
            "goal": goal,
            "steps": steps,
            "total_sigma": round(total_sigma, 4),
            "feasible": total_sigma < 0.5,
            "n_steps": len(steps),
        }

    def commonsense_check(self, statement: str, context: str = "") -> Dict[str, Any]:
        """Graph-grounded sketch + gate fallback; high σ on a stored triple vs claim → implausible."""
        stmt = str(statement).strip()
        ctx = str(context).strip()

        g = self.graph
        if g is not None and hasattr(g, "entities") and callable(g.entities):
            words = stmt.lower().split()
            entities_list = list(g.entities())
            entities = [e for e in entities_list if e.lower() in stmt.lower()]
            if entities and hasattr(g, "relations_of"):
                for entity in entities:
                    rels = g.relations_of(entity)
                    for rel in rels:
                        contradiction = f"{rel['subject']} {rel['relation']} {rel['object']}"
                        σ_check, _ = self.gate.score(contradiction, stmt)
                        if float(σ_check) > 0.7:
                            return {
                                "plausible": False,
                                "σ": round(float(σ_check), 4),
                                "sigma": round(float(σ_check), 4),
                                "contradiction": contradiction,
                                "verdict": "RETHINK",
                            }

        σ, verdict = self.gate.score(ctx or "common sense", stmt)
        vn = _verdict_str(verdict)
        sf = round(float(σ), 4)
        return {
            "plausible": vn != "ABSTAIN",
            "σ": sf,
            "sigma": sf,
            "verdict": vn,
        }

    def predict(self, current_state: str, action: str) -> Dict[str, Any]:
        """Predict outcome of *action* in *current_state* (template response + σ)."""
        cs, act = str(current_state).strip(), str(action).strip()
        prompt = f"State: {cs}. Action: {act}. What happens?"
        response = f"After {act}, the state changes"
        σ, verdict = self.gate.score(prompt, response)
        vn = _verdict_str(verdict)
        return {
            "predicted_outcome": f"Effect of {act} on {cs}",
            "σ": round(float(σ), 4),
            "confidence": round(1.0 - float(σ), 4),
            "verdict": vn,
        }

    def simulate(self, initial_state: str, actions: Sequence[str]) -> Dict[str, Any]:
        """Roll out a list of actions; accumulate σ along the trajectory."""
        state = str(initial_state).strip()
        trajectory: List[Dict[str, Any]] = []
        cumulative_σ = 0.0
        acts = list(actions)

        for action in acts:
            result = self.predict(state, str(action))
            sg = float(result["σ"])
            cumulative_σ += sg
            trajectory.append(
                {
                    "state": state,
                    "action": str(action),
                    "σ": sg,
                    "cumulative_σ": round(cumulative_σ, 4),
                }
            )
            state = str(result["predicted_outcome"])

        return {
            "trajectory": trajectory,
            "final_σ": round(cumulative_σ / max(len(acts), 1), 4),
            "steps": len(acts),
        }

    def _validate_predictions(self, actual: str) -> None:
        for pred in self.predictions:
            if "validated" not in pred:
                pred["validated"] = True
                pred["actual"] = actual[:50]

    def _analyze_feature_trend(self) -> Dict[str, str]:
        if len(self.states) < 2:
            return {}
        recent = self.states[-5:]
        all_keys: set[str] = set()
        for s in recent:
            all_keys.update(s.features.keys())
        trends: Dict[str, str] = {}
        for key in all_keys:
            values = [s.features.get(key) for s in recent if key in s.features]
            if len(values) >= 2 and all(isinstance(v, (int, float)) for v in values):
                trend = float(values[-1]) - float(values[0])
                if trend > 0:
                    trends[key] = "rising"
                elif trend < 0:
                    trends[key] = "falling"
                else:
                    trends[key] = "stable"
        return trends

    def predict_next_state(self, hint: str = "") -> Dict[str, Any]:
        if not self.states:
            return {"predicted_description": None, "sigma": 1.0, "verdict": "ABSTAIN", "feature_trend": {}}
        tr = self._analyze_feature_trend()
        last = self.states[-1]
        chunks = [last.description]
        for key, direction in tr.items():
            fv = last.features.get(key)
            if isinstance(fv, (int, float)):
                step = 0.1 if direction == "rising" else -0.1 if direction == "falling" else 0.0
                chunks.append(f"{key} ~{fv + step} ({direction})")
        desc = "; ".join(chunks) + (f" | {hint}" if hint else "")
        sigma, verdict = self.gate.score("predict_next_world_state", desc[:1500])
        return {
            "predicted_description": desc,
            "sigma": round(float(sigma), 4),
            "verdict": str(verdict),
            "feature_trend": tr,
        }
