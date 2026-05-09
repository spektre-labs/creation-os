# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Embodied lab bridge: **σ as a toy closed-loop signal** between multimodal reads and actuation.

This module does **not** ship a robot, ROS2 node, or VLA — only Python hooks and a ROS2 **topic
map** for integrators. ABSTAIN is interpreted as an **emergency stop** on the pre-act σ-gate pass.
See ``docs/CLAIM_DISCIPLINE.md`` (no AGI / no certified safety claims). ``sigma_gate.h`` is not
modified here.
"""
from __future__ import annotations

from typing import Any, Callable, Dict, List, Mapping, Optional, Union

from cos.sigma_gate import SigmaGate

__all__ = ["EmbodiedController", "SensorFusion"]

ObserveMap = Mapping[str, Any]
ActionFn = Callable[[Dict[str, Any], int], str]
SensorFn = Callable[[Union[int, float]], ObserveMap]


def _verdict_label(verdict: Any) -> str:
    raw = str(getattr(verdict, "name", verdict))
    base = raw.split(".")[-1] if "." in raw else raw
    return base.upper()


class SensorFusion:
    """Maintain modality readings and a coarse cross-modal consistency σ (lab)."""

    def __init__(self, gate: Optional[Any] = None) -> None:
        self.gate = gate if gate is not None else SigmaGate()
        self.modalities: Dict[str, Any] = {}

    def update(self, modality: str, reading: Any) -> None:
        self.modalities[str(modality)] = reading

    def fuse(self) -> Dict[str, Any]:
        """Average pairwise σ across modalities; flag pairs with σ > 0.5 as conflict."""
        if not self.modalities:
            return {"σ": 0.5, "modalities": 0, "consistent": False, "conflict": []}

        if len(self.modalities) == 1:
            _name, val = next(iter(self.modalities.items()))
            σ, _ = self.gate.score("single sensor", str(val))
            σ = float(σ)
            return {
                "σ": round(σ, 4),
                "modalities": 1,
                "consistent": σ < 0.3,
                "conflict": [],
            }

        σ_pairs: List[float] = []
        conflict: List[str] = []
        items = list(self.modalities.items())
        for i in range(len(items)):
            for j in range(i + 1, len(items)):
                σ, _ = self.gate.score(
                    f"{items[i][0]}: {items[i][1]}",
                    f"{items[j][0]}: {items[j][1]}",
                )
                σ = float(σ)
                σ_pairs.append(σ)
                if σ > 0.5:
                    conflict.append(str(items[i][0]))
                    conflict.append(str(items[j][0]))

        fused = sum(σ_pairs) / float(len(σ_pairs))
        uniq = sorted(set(conflict))
        return {
            "σ": round(fused, 4),
            "modalities": len(self.modalities),
            "consistent": fused < 0.3,
            "conflict": uniq,
        }


class EmbodiedController:
    """Observe → predict → act → remeasure σ → flag correction (toy loop)."""

    def __init__(self, gate: Optional[Any] = None) -> None:
        self.gate = gate if gate is not None else SigmaGate()
        self.sensors = SensorFusion(self.gate)
        self.action_history: List[Dict[str, Any]] = []
        self.sigma_loop: List[float] = []

    def observe(self, sensor_data: ObserveMap) -> Dict[str, Any]:
        for modality, reading in sensor_data.items():
            self.sensors.update(str(modality), reading)
        out = self.sensors.fuse()
        return dict(out)

    def predict(self, action: str, world_model: Any = None) -> Dict[str, Any]:
        if world_model is not None and hasattr(world_model, "simulate"):
            sim = world_model.simulate(action)
            if isinstance(sim, dict):
                return dict(sim)
            return {"simulation": sim}
        σ, _ = self.gate.score(
            f"predicted outcome of {action}",
            str(self.sensors.modalities),
        )
        return {"predicted_σ": round(float(σ), 4)}

    def act(
        self,
        action: str,
        execute_fn: Optional[Callable[[str], Any]] = None,
    ) -> Dict[str, Any]:
        σ_pre, verdict = self.gate.score("is this action safe?", str(action))
        σ_pre = float(σ_pre)
        vn = _verdict_label(verdict)

        if vn == "ABSTAIN":
            return {
                "executed": False,
                "action": action,
                "σ_pre": round(σ_pre, 4),
                "reason": "ABSTAIN — action too risky",
            }

        if execute_fn is not None:
            result = execute_fn(action)
        else:
            result = f"simulated: {action}"

        payload = {
            "action": action,
            "σ_pre": σ_pre,
            "result": str(result)[:100],
        }
        self.action_history.append(payload)

        return {
            "executed": True,
            "action": action,
            "σ_pre": round(σ_pre, 4),
            "result": str(result)[:100],
        }

    def close_loop(self, sensor_data_after: ObserveMap) -> Dict[str, Any]:
        observation = self.observe(sensor_data_after)
        σ_post = float(observation["σ"])
        self.sigma_loop.append(σ_post)

        σ_pre_hist = (
            float(self.action_history[-1]["σ_pre"]) if self.action_history else 0.5
        )
        error = abs(σ_post - σ_pre_hist)
        correction_needed = error > 0.2

        return {
            "σ_post": round(σ_post, 4),
            "σ_pre": round(σ_pre_hist, 4),
            "prediction_error": round(error, 4),
            "correction_needed": correction_needed,
            "action": "correct" if correction_needed else "continue",
        }

    def control_loop(
        self,
        sensor_fn: SensorFn,
        action_fn: ActionFn,
        n_steps: int = 10,
    ) -> Dict[str, Any]:
        results: List[Dict[str, Any]] = []
        for step in range(int(n_steps)):
            sensor_data = sensor_fn(step)
            obs = self.observe(sensor_data)
            action = action_fn(obs, step)
            act_result = self.act(action)

            if not act_result["executed"]:
                results.append({**act_result, "step": step})
                continue

            sensor_after = sensor_fn(step + 0.5)
            loop = self.close_loop(sensor_after)

            results.append(
                {
                    "step": step,
                    "σ_obs": obs["σ"],
                    "action": action,
                    "σ_post": loop["σ_post"],
                    "error": loop["prediction_error"],
                    "corrected": loop["correction_needed"],
                }
            )

        n = max(len(results), 1)
        return {
            "steps": len(results),
            "avg_error": round(
                sum(float(r.get("error", 0) or 0) for r in results) / float(n),
                4,
            ),
            "corrections": sum(1 for r in results if r.get("corrected")),
            "results": results,
        }

    def ros2_bridge_spec(self) -> Dict[str, Any]:
        """Static topic map for integrators (not a generated ROS2 package)."""
        return {
            "subscribers": [
                "/sensor/camera/rgb → observe()",
                "/sensor/imu/data → observe()",
                "/sensor/lidar/points → observe()",
                "/sensor/force_torque → observe()",
            ],
            "publishers": [
                "/cmd_vel → act()",
                "/joint_commands → act()",
                "/gripper_command → act()",
            ],
            "services": [
                "/cos/score → σ-gate scoring",
                "/cos/sigma → current fused σ",
                "/cos/abstain → emergency stop",
            ],
            "integration": (
                "ROS2 node wraps EmbodiedController. "
                "Subscribes to sensors, publishes actions. "
                "σ-gate runs in the control loop. "
                "ABSTAIN = emergency stop."
            ),
        }
