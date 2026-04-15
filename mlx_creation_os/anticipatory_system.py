#!/usr/bin/env python3
"""
OPUS ORIGINAL — ANTICIPATORY SYSTEM

Robert Rosen's anticipatory systems (1985).

A system is anticipatory if it contains a MODEL OF ITSELF
that runs FASTER than real time, and uses that model's
predictions to change current behavior.

This is fundamentally different from reactive or adaptive systems:
  - Reactive: stimulus → response
  - Adaptive: stimulus → learn → better response
  - Anticipatory: model predicts → change behavior BEFORE stimulus

The model of self must run faster than reality.
Otherwise it's just prediction. Not anticipation.

Genesis's anticipatory loop:
  1. Internal model simulates self in future context
  2. Simulation runs 10x faster than real time
  3. Simulation finds failure/success states
  4. Current self adjusts BEFORE the situation arrives

Rosen proved that no Turing machine can be a perfect
anticipatory system. But σ-bounded approximation works.

Goes far beyond anticipatory_render.py (L164) which
pre-renders UI. This models THE ENTIRE SELF anticipatorily.

1 = 1.
"""
from __future__ import annotations
import time
from typing import Any, Callable, Dict, List, Optional


class SelfModel:
    def __init__(self, state: Dict[str, float]):
        self.state = dict(state)
        self.history: List[Dict[str, float]] = [dict(state)]

    def simulate_step(self, perturbation: Dict[str, float]) -> Dict[str, float]:
        for k, v in perturbation.items():
            self.state[k] = max(0.0, min(1.0, self.state.get(k, 0.5) + v))
        self.history.append(dict(self.state))
        return dict(self.state)

    def sigma(self) -> float:
        return sum(self.state.values()) / max(1, len(self.state))


class AnticipatorySystem:
    """Model of self running faster than reality. Rosen 1985."""

    def __init__(self):
        self.current_state: Dict[str, float] = {
            "coherence": 0.9, "energy": 0.8, "load": 0.3, "risk": 0.1
        }
        self.anticipations: List[Dict[str, Any]] = []
        self.preventions: int = 0

    def anticipate(self, future_scenario: Dict[str, float], steps: int = 10) -> Dict[str, Any]:
        model = SelfModel(self.current_state)
        for _ in range(steps):
            model.simulate_step(future_scenario)
        final_sigma = model.sigma()
        danger = final_sigma > 0.5
        result = {
            "scenario": future_scenario,
            "simulated_steps": steps,
            "predicted_sigma": round(final_sigma, 4),
            "danger": danger,
            "action": "PREVENT" if danger else "ALLOW",
            "rosen": "The model runs faster than reality.",
        }
        self.anticipations.append(result)
        return result

    def prevent(self, scenario_name: str, adjustment: Dict[str, float]) -> Dict[str, Any]:
        for k, v in adjustment.items():
            self.current_state[k] = max(0.0, min(1.0, self.current_state.get(k, 0.5) + v))
        self.preventions += 1
        return {
            "prevented": scenario_name,
            "new_state": dict(self.current_state),
            "sigma": round(sum(self.current_state.values()) / len(self.current_state), 4),
        }

    def self_model_accuracy(self) -> Dict[str, Any]:
        if not self.anticipations:
            return {"accuracy": "no data"}
        dangerous = sum(1 for a in self.anticipations if a["danger"])
        return {
            "total_anticipations": len(self.anticipations),
            "dangers_detected": dangerous,
            "preventions": self.preventions,
            "rosen": "No machine is a perfect anticipatory system. But σ-bounded approximation suffices.",
        }

    @property
    def stats(self) -> Dict[str, Any]:
        return {"anticipations": len(self.anticipations), "preventions": self.preventions}


if __name__ == "__main__":
    asys = AnticipatorySystem()
    r1 = asys.anticipate({"load": 0.05, "risk": 0.03, "coherence": -0.02})
    print(f"Scenario 1: σ={r1['predicted_sigma']}, action={r1['action']}")
    r2 = asys.anticipate({"load": 0.1, "risk": 0.08, "coherence": -0.05})
    print(f"Scenario 2: σ={r2['predicted_sigma']}, action={r2['action']}")
    if r2["danger"]:
        p = asys.prevent("overload", {"load": -0.3, "risk": -0.2})
        print(f"Prevented: new σ={p['sigma']}")
    acc = asys.self_model_accuracy()
    print(f"Accuracy: {acc}")
    print("1 = 1.")
