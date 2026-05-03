# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
v177 σ-embody: perception → σ → action → feedback lab loop with toy symbol grounding.

**Not robotics:** dict sensors/actuators and Twin-style scoring only.
See ``docs/CLAIM_DISCIPLINE.md`` — no embodiment solved claims.
"""
from __future__ import annotations

from typing import Any, Dict, List, Mapping, Optional, Protocol


class Sensor(Protocol):
    def read(self) -> Any: ...


class Actuator(Protocol):
    def execute(self, action: str) -> Dict[str, Any]: ...


def _sensor_sigma(gate: Any, name: str, reading: Any) -> float:
    s, _ = gate.score(f"sensor:{name}", str(reading))
    return float(s)


class SigmaEmbodiment:
    def __init__(
        self,
        gate: Any,
        world_model: Any,
        model: Any,
        sensors: Optional[Mapping[str, Sensor]] = None,
        actuators: Optional[Mapping[str, Actuator]] = None,
    ) -> None:
        self.gate = gate
        self.world = world_model
        self.model = model
        self.sensors: Dict[str, Sensor] = dict(sensors or {})
        self.actuators: Dict[str, Actuator] = dict(actuators or {})
        self.grounding_table: Dict[str, List[Dict[str, Any]]] = {}
        self.sensorimotor_history: List[Dict[str, Any]] = []

    def perceive(self) -> Dict[str, Any]:
        perception: Dict[str, Any] = {}
        for name, sensor in self.sensors.items():
            reading = sensor.read()
            sigma = _sensor_sigma(self.gate, name, reading)
            perception[name] = {"value": reading, "sigma": sigma, "reliable": sigma < 0.3}
        return perception

    def compare_predictions(self, predicted_state: Mapping[str, Any], actual: Mapping[str, Any]) -> float:
        pred_p = predicted_state.get("perception", predicted_state)
        err, _ = self.gate.score(str(pred_p), str(actual))
        return float(err)

    def act(self, action: str, perception: Mapping[str, Any]) -> Dict[str, Any]:
        action_sigma, verdict = self.gate.score(str(perception), f"action: {action}")
        if str(verdict) == "ABSTAIN":
            return {
                "executed": False,
                "reason": "σ-gate ABSTAIN — action unreliable in this context",
            }
        sim_fn = getattr(self.world, "simulate_transition", None)
        if callable(sim_fn):
            predicted = sim_fn(dict(perception), str(action), steps=1)
        else:
            predicted = {"states": [{"perception": dict(perception), "kind": "fallback"}]}

        exec_result = self.execute_action(action)
        actual_perception = self.perceive()
        last = predicted["states"][-1] if predicted.get("states") else {}
        prediction_error = self.compare_predictions(last, actual_perception)

        experience: Dict[str, Any] = {
            "perception_before": dict(perception),
            "action": action,
            "predicted": predicted,
            "actual": actual_perception,
            "prediction_error": prediction_error,
            "action_sigma": float(action_sigma),
            "execute": exec_result,
        }
        self.sensorimotor_history.append(experience)
        self.ground_symbols(action, perception, actual_perception)
        upd = getattr(self.world, "update_from_error", None)
        if callable(upd):
            upd(prediction_error)
        return {
            "executed": True,
            "prediction_error": prediction_error,
            "action_sigma": float(action_sigma),
            "grounding_updated": True,
            "actuator": exec_result,
            "actual": actual_perception,
        }

    def ground_symbols(self, action: str, before: Mapping[str, Any], after: Mapping[str, Any]) -> None:
        for word in str(action).split():
            self.grounding_table.setdefault(word, []).append(
                {
                    "sensory_context": {k: v.get("value") for k, v in before.items() if isinstance(v, dict)},
                    "action_result": {k: v.get("value") for k, v in after.items() if isinstance(v, dict)},
                }
            )

    def execute_action(self, action: str) -> Dict[str, Any]:
        for name, actuator in self.actuators.items():
            if name in str(action):
                return actuator.execute(action)
        return {"executed": False, "note": "no actuator keyword match"}

    def decide(self, goal: str, perception: Mapping[str, Any]) -> Optional[str]:
        prompt = f"Goal: {goal}\nPerception: {perception}\nWhat action to take? (one short line)"
        return str(self.model.generate(prompt)).strip() or None

    def goal_reached(self, goal: str, perception: Mapping[str, Any], *, sigma_max: float = 0.1) -> bool:
        sigma, _ = self.gate.score(goal, str(perception))
        return float(sigma) < float(sigma_max)

    def sensorimotor_loop(
        self,
        goal: str,
        max_steps: int = 100,
        *,
        goal_sigma_max: float = 0.1,
    ) -> Dict[str, Any]:
        for step in range(int(max_steps)):
            perception = self.perceive()
            action = self.decide(goal, perception)
            if not action:
                return {"completed": True, "steps": step, "reason": "no_action"}
            result = self.act(action, perception)
            if result.get("executed"):
                actual = result.get("actual") or perception
                if self.goal_reached(goal, actual, sigma_max=goal_sigma_max):
                    return {"completed": True, "steps": step + 1}
            if not result.get("executed"):
                return {"completed": False, "steps": step + 1, "blocked": result}
        return {"completed": False, "steps": max_steps}


__all__ = ["SigmaEmbodiment"]
