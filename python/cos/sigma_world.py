# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
v172 σ-world: toy physical rollout + σ-trust per transition (lab).

**Not a differentiable physics engine:** constants are illustrative only.
"""
from __future__ import annotations

from typing import Any, Callable, Dict, List, Optional


class SigmaWorldModel:
    def __init__(self, gate: Any, jepa: Any, model: Optional[Any] = None) -> None:
        self.gate = gate
        self.jepa = jepa
        self.model = model
        self.physics_rules = self.load_physics()
        self.temporal_buffer: List[Dict[str, Any]] = []

    def load_physics(self) -> Dict[str, Any]:
        return {
            "gravity": {"acceleration": 9.81, "unit": "m/s²"},
            "conservation_energy": True,
            "object_permanence": True,
            "causality": "forward_only",
        }

    def parse_initial_state(self, scenario: str) -> Dict[str, Any]:
        return {"scenario": scenario, "t": 0.0, "y": 10.0, "v": 0.0}

    def apply_physics(self, state: Dict[str, Any]) -> Dict[str, Any]:
        g = float(self.physics_rules["gravity"]["acceleration"])
        dt = 0.1
        y = float(state.get("y", 0.0))
        v = float(state.get("v", 0.0))
        v2 = v - g * dt
        y2 = y + v * dt
        return {"scenario": state.get("scenario"), "t": float(state.get("t", 0.0)) + dt, "y": y2, "v": v2}

    def _jepa_predict(self, current: Dict[str, Any]) -> Dict[str, Any]:
        pred_fn: Optional[Callable[..., Any]] = getattr(self.jepa, "world_model_predict", None)
        if callable(pred_fn):
            try:
                return {"jepa": pred_fn(current)}
            except Exception:
                return {"jepa": "unavailable"}
        enc_fn = getattr(self.jepa, "encode_state", None)
        if callable(enc_fn):
            try:
                return {"jepa_latent": enc_fn(current)}
            except Exception:
                return {"jepa": "encode_failed"}
        return {"jepa": "stub"}

    def simulate(self, scenario: str, steps: int = 10) -> Dict[str, Any]:
        states: List[Dict[str, Any]] = [self.parse_initial_state(scenario)]
        for step in range(int(steps)):
            current = states[-1]
            next_state = self.apply_physics(current)
            next_state["jepa_hint"] = self._jepa_predict(current)
            sigma, verdict = self.gate.score(str(current), str(next_state))
            next_state["sigma"] = float(sigma)
            next_state["step"] = step + 1
            states.append(next_state)
            if str(verdict) == "ABSTAIN":
                next_state["note"] = "simulation unreliable beyond this point"
                break
        reliable = all(float(s.get("sigma", 1.0)) < 0.3 for s in states[1:])
        return {
            "states": states,
            "steps_simulated": max(0, len(states) - 1),
            "final_sigma": float(states[-1].get("sigma", 1.0)),
            "reliable": reliable,
        }

    def simulate_transition(
        self,
        perception: Dict[str, Any],
        action: str,
        *,
        steps: int = 1,
    ) -> Dict[str, Any]:
        """
        v177 embodiment hook: one-step ``perception + action → predicted perception`` (lab).

        Uses the same gate stress path as :meth:`simulate`, but keys are sensor-shaped dicts.
        """
        states: List[Dict[str, Any]] = [
            {"perception": {k: dict(v) if isinstance(v, dict) else v for k, v in perception.items()}, "action": str(action), "step": 0, "kind": "pre"}
        ]
        _ = int(steps)
        pred: Dict[str, Any] = {}
        for k, v in perception.items():
            if isinstance(v, dict):
                pv = dict(v)
                if "value" in pv:
                    pv["value"] = f"{pv.get('value')}|act:{str(action)[:24]}"
                pred[k] = pv
            else:
                pred[k] = v
        sigma, verdict = self.gate.score(str(perception), str(pred))
        states.append(
            {
                "perception": pred,
                "action": str(action),
                "step": 1,
                "sigma": float(sigma),
                "verdict": verdict,
                "kind": "predicted",
            }
        )
        return {
            "states": states,
            "steps_simulated": len(states) - 1,
            "final_sigma": float(states[-1].get("sigma", 1.0)),
        }

    def update_from_error(self, prediction_error: float) -> None:
        """Append prediction-error scalar for audit / future world-model hooks (lab)."""
        self.temporal_buffer.append({"prediction_error": float(prediction_error)})

    def causal_chain(self, event: str, max_depth: int = 5) -> List[Dict[str, Any]]:
        if self.model is None:
            raise ValueError("causal_chain requires a text model with generate()")
        chain: List[Dict[str, Any]] = [{"event": event, "depth": 0, "sigma": 0.0}]
        for depth in range(int(max_depth)):
            current = str(chain[-1]["event"])
            consequence = self.model.generate(f"What is the most likely consequence of: {current}?")
            sigma, verdict = self.gate.score(current, consequence)
            chain.append({"event": consequence, "depth": depth + 1, "sigma": float(sigma), "verdict": verdict})
            if float(sigma) > 0.5:
                chain[-1]["note"] = "causal chain unreliable beyond this point"
                break
        return chain


__all__ = ["SigmaWorldModel"]
