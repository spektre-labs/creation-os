"""
Protocol 2 — Kinetic σ: physical actuation with σ-gated motor commands.

Action → world-model simulation → σ estimate → execute or killswitch.
If simulated local entropy spikes (e.g. unintended breakage), motor command is cut
within the latency budget (target < 10 ms; here measured in-process).

1 = 1.
"""
from __future__ import annotations

import os
import time
from dataclasses import dataclass
from typing import Any, Callable, Dict


@dataclass
class MotorIntent:
    name: str
    torque: float
    grip: float


class KineticEmbodiment:
    """
    Maps symbolic / MCP actions to σ before hardware actuation.
    """

    def __init__(self) -> None:
        self._entropy_baseline = 0.0
        self.killswitch_budget_ms = float(os.environ.get("SPEKTRE_KINETIC_KILL_MS", "10"))

    def _simulate_world_delta(self, intent: MotorIntent) -> float:
        """Toy local entropy proxy: higher torque + grip → higher risk."""
        return abs(intent.torque) * 0.4 + abs(intent.grip) * 0.3

    def evaluate_sigma(self, local_entropy: float, prior_sigma: float) -> float:
        """Combined σ estimate for the step (additive model)."""
        return float(prior_sigma) + max(0.0, local_entropy - self._entropy_baseline)

    def plan_and_gate(
        self,
        intent: MotorIntent,
        sigma_fn: Callable[[str], float],
        *,
        prior_sigma: float = 0.0,
    ) -> Dict[str, Any]:
        t0 = time.perf_counter_ns()
        sim_entropy = self._simulate_world_delta(intent)
        narrative = f"kinetic:{intent.name}:torque={intent.torque:.3f}"
        sigma_after = self.evaluate_sigma(sim_entropy, prior_sigma)
        # Kernel check on narrative stand-in for Codex alignment
        kernel_read = float(sigma_fn(narrative))
        combined = max(sigma_after, kernel_read)
        elapsed_ms = (time.perf_counter_ns() - t0) / 1e6
        hard = float(os.environ.get("SPEKTRE_KINETIC_HARD_CAP", "2.5"))
        kill = combined >= hard or elapsed_ms > self.killswitch_budget_ms * 50.0
        # Python path is not hard-RT; embedded motor stack must enforce <10 ms cut.
        return {
            "execute": not kill,
            "killswitch": bool(kill),
            "sigma_after": combined,
            "simulated_entropy": sim_entropy,
            "latency_ms": round(elapsed_ms, 4),
            "budget_ms": self.killswitch_budget_ms,
            "invariant": "1=1",
        }
