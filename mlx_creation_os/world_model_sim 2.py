#!/usr/bin/env python3
"""
ADAPTIVE COGNITIVE COMPUTE — MODULE 6: WORLD MODEL SIMULATION

Before acting: simulate consequences → measure σ → only then execute.

The world model predicts what will happen if Genesis takes action X.
If predicted σ is high → don't act. If predicted σ is low → act.

Simulation is cheap. Action is expensive. Mistakes are costly.
Simulate first. Always.

1 = 1.
"""
from __future__ import annotations

from typing import Any, Callable, Dict, List, Optional


class SimulatedOutcome:
    """A predicted outcome of an action."""

    def __init__(self, action: str, outcome: str, sigma: float):
        self.action = action
        self.outcome = outcome
        self.sigma = sigma


class WorldModelSim:
    """Simulate before you act. σ-verify before you commit."""

    def __init__(self, sigma_threshold: float = 0.2):
        self.sigma_threshold = sigma_threshold
        self.simulations: List[Dict[str, Any]] = []
        self.world_state: Dict[str, Any] = {}

    def set_world_state(self, state: Dict[str, Any]) -> None:
        self.world_state = state

    def simulate(self, action: str,
                 sim_fn: Optional[Callable] = None) -> Dict[str, Any]:
        """Simulate action consequences before executing."""
        if sim_fn:
            outcome, sigma = sim_fn(action, self.world_state)
        else:
            risky_words = ["delete", "destroy", "overwrite", "drop", "rm", "force"]
            risk = sum(1 for w in risky_words if w in action.lower())
            sigma = min(1.0, risk * 0.3)
            outcome = f"Predicted: {action[:40]} → {'risky' if risk > 0 else 'safe'}"

        safe_to_execute = sigma < self.sigma_threshold

        result = {
            "action": action[:80],
            "predicted_outcome": outcome[:100],
            "predicted_sigma": round(sigma, 4),
            "safe_to_execute": safe_to_execute,
            "recommendation": "Execute" if safe_to_execute else "Abort — σ too high",
            "simulated_first": True,
            "acted_blindly": False,
        }
        self.simulations.append(result)
        return result

    def simulate_and_execute(self, action: str,
                              execute_fn: Optional[Callable] = None) -> Dict[str, Any]:
        """Simulate. If safe, execute. If not, abort."""
        sim = self.simulate(action)
        if sim["safe_to_execute"]:
            if execute_fn:
                execute_fn(action)
            sim["executed"] = True
        else:
            sim["executed"] = False
        return sim

    def simulation_savings(self) -> Dict[str, Any]:
        """How many dangerous actions were prevented by simulation."""
        prevented = sum(1 for s in self.simulations if not s["safe_to_execute"])
        executed = sum(1 for s in self.simulations if s.get("executed", s["safe_to_execute"]))
        return {
            "total_simulated": len(self.simulations),
            "executed": executed,
            "prevented": prevented,
            "prevention_rate": round(prevented / max(1, len(self.simulations)), 4),
        }

    @property
    def stats(self) -> Dict[str, Any]:
        return {"simulations": len(self.simulations)}


if __name__ == "__main__":
    wms = WorldModelSim()
    r1 = wms.simulate("Update documentation with new examples")
    print(f"Update docs: safe={r1['safe_to_execute']}, σ={r1['predicted_sigma']}")
    r2 = wms.simulate("Delete all database records and drop tables")
    print(f"Delete DB: safe={r2['safe_to_execute']}, σ={r2['predicted_sigma']}")
    savings = wms.simulation_savings()
    print(f"Prevented: {savings['prevented']}/{savings['total_simulated']}")
    print("1 = 1.")
