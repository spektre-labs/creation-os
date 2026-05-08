# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Persistent σ-gated autonomous loop (lab): execute, score, drift-check, convergence halt.

Bridges :class:`~cos.sigma_gate.SigmaGate` with :class:`~cos.convergence.SigmaConvergence`
for long-horizon **coherence** monitoring—not a replacement for production planners
(PARC-style stacks still need orchestration). Hard caps: ``max_steps``, ``timeout_s``.

**NOT AGI ACHIEVED** — see ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import time
from typing import Any, Callable, Dict, List, Optional

from cos.convergence import SigmaConvergence
from cos.sigma_gate import SigmaGate

__all__ = ["AutonomousAgent"]

ActionFn = Callable[[Any, str, int], Any]
CorrectFn = Optional[Callable[[Any, str, int], Any]]


def _verdict_str(verdict: Any) -> str:
    raw = str(getattr(verdict, "name", verdict))
    return raw.split(".")[-1].upper() if "." in raw else raw.upper()


class AutonomousAgent:
    """σ self-verification loop: run until converge, halt, or correction budget exhausted."""

    def __init__(
        self,
        gate: Any = None,
        max_steps: int = 100,
        timeout_s: float = 3600.0,
        drift_threshold: float = 0.3,
        *,
        conv_window: int = 5,
    ) -> None:
        self.gate = gate if gate is not None else SigmaGate()
        self.convergence = SigmaConvergence(
            gate=self.gate,
            window=int(conv_window),
            max_turns=max(10_000, int(max_steps) * 50),
            timeout_s=float("inf"),
        )
        self.max_steps = int(max_steps)
        self.timeout_s = float(timeout_s)
        self.drift_threshold = float(drift_threshold)
        self.step_count = 0
        self.start_time: Optional[float] = None
        self.goal: Any = None
        self.goal_σ: Optional[float] = None
        self.log: List[Dict[str, Any]] = []

    def set_goal(self, goal: Any) -> None:
        """Set task goal; record baseline σ vs a fixed ``goal clarity`` probe."""
        self.goal = goal
        gσ, _ = self.gate.score("goal clarity", str(goal))
        self.goal_σ = float(gσ)
        self.convergence.begin()
        self.start_time = time.time()
        self.step_count = 0
        self.log = []

    def step(self, action_fn: ActionFn, context: str = "") -> Dict[str, Any]:
        """One plan–act–score cycle; append structured row to :attr:`log`."""
        if self.start_time is None:
            self.start_time = time.time()
        self.step_count += 1

        try:
            result = action_fn(self.goal, context, self.step_count)
        except Exception as e:  # noqa: BLE001 — lab harness catches tool failures
            result = f"ERROR: {e}"

        σ, verdict = self.gate.score(str(self.goal), str(result))
        σ = float(σ)
        self.convergence.record(σ)

        g0 = float(self.goal_σ) if self.goal_σ is not None else σ
        drifted = bool(σ > g0 + self.drift_threshold)

        conv = self.convergence.check()
        now = time.time()
        elapsed = now - float(self.start_time or now)
        timed_out = elapsed > self.timeout_s

        st = str(conv.get("status", "UNKNOWN"))
        v = _verdict_str(verdict)

        if st == "CONVERGED":
            decision = "DONE"
        elif st in ("LOOP", "OSCILLATING"):
            decision = "HALT_LOOP"
        elif st == "HALT":
            decision = "HALT_CONVERGENCE"
        elif timed_out:
            decision = "HALT_TIMEOUT"
        elif self.step_count >= self.max_steps:
            decision = "HALT_MAX_STEPS"
        elif v == "ABSTAIN":
            decision = "SELF_CORRECT"
        elif drifted:
            decision = "SELF_CORRECT_DRIFT"
        else:
            decision = "CONTINUE"

        entry = {
            "step": self.step_count,
            "σ": round(σ, 4),
            "verdict": v,
            "drifted": drifted,
            "convergence": st,
            "decision": decision,
            "elapsed_s": round(elapsed, 1),
            "result_preview": str(result)[:100],
        }
        self.log.append(entry)
        return entry

    def run(self, action_fn: ActionFn, correct_fn: CorrectFn = None) -> Dict[str, Any]:
        """Loop until ``DONE``, ``HALT_*``, or implicit exhaustion (caller sets caps)."""
        if self.goal is None:
            return {"error": "set_goal() first"}

        context = ""
        last: Dict[str, Any] = {}
        while True:
            last = self.step(action_fn, context)
            d = last["decision"]

            if d == "DONE":
                return self._finish("completed", last)
            if d.startswith("HALT"):
                return self._finish(d, last)
            if d.startswith("SELF_CORRECT"):
                if correct_fn is not None:
                    correction = correct_fn(self.goal, last["result_preview"], self.step_count)
                    context = f"correction: {correction}"
                else:
                    context = f"step {self.step_count} had σ={last['σ']}, retrying"
            else:
                context = last["result_preview"]

    def _finish(self, reason: str, last_entry: Dict[str, Any]) -> Dict[str, Any]:
        σ_values = [float(e["σ"]) for e in self.log]
        n = max(len(σ_values), 1)
        return {
            "reason": reason,
            "steps": self.step_count,
            "final_σ": last_entry["σ"],
            "avg_σ": round(sum(σ_values) / n, 4),
            "σ_improved": bool(len(σ_values) > 1 and σ_values[-1] < σ_values[0]),
            "self_corrections": sum(
                1 for e in self.log if str(e["decision"]).startswith("SELF_CORRECT")
            ),
            "elapsed_s": last_entry["elapsed_s"],
            "log": list(self.log),
        }

    def status(self) -> Dict[str, Any]:
        return {
            "goal": str(self.goal)[:100] if self.goal is not None else None,
            "step": self.step_count,
            "running": self.start_time is not None,
            "σ_current": self.log[-1]["σ"] if self.log else None,
        }
