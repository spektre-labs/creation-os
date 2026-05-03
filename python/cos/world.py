# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-world — lightweight world-model lab harness (JEPA-style *idea*, not a trained JEPA).

Track observations, run coarse “what if” simulations scored by ``SigmaGate``, and
iterate a trivial plan loop. This does **not** train latent encoders or claim
embedding-level prediction quality; see ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import time
from typing import Any, Dict, List, Optional


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
    """Observe → predict (heuristic) → simulate (gate) → plan (gate loop)."""

    def __init__(self, gate: Any = None, max_history: int = 100) -> None:
        from cos.sigma_gate import SigmaGate

        self.gate = gate or SigmaGate()
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

    def predict(self, query: str, n_steps: int = 1) -> Dict[str, Any]:
        """Heuristic forecast metadata from recent state text + numeric feature drift."""
        del n_steps  # reserved for richer policies
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

    def simulate(self, action: str, current_state: Optional[WorldState] = None) -> Dict[str, Any]:
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
            sim = self.simulate(step_desc)
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

    def commonsense_check(self, claim: str) -> Dict[str, Any]:
        sigma, verdict = self.gate.score("commonsense_claim", str(claim)[:2000])
        return {"claim": claim, "sigma": round(float(sigma), 4), "verdict": str(verdict)}

    def predict_next_state(self, hint: str = "") -> Dict[str, Any]:
        """Heuristic successor narrative from trend tags + last observation."""
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


__all__ = ["SigmaWorld", "WorldState"]
