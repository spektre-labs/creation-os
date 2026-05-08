# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Loop detection via σ-sequence statistics (lab heuristic, not a formal proof).

Uses measured σ from :class:`~cos.sigma_gate.SigmaGate` (or caller-supplied floats)
over a sliding window: small spread with high σ ⇒ treat as LOOP; strict alternation
with moderate σ ⇒ OSCILLATING; small spread with low σ ⇒ CONVERGED. Hard caps:
``max_turns`` and ``timeout_s``. See ``docs/CLAIM_DISCIPLINE.md`` — evidence class: lab.
"""
from __future__ import annotations

import time
from typing import Any, Callable, Dict, List, Optional

__all__ = ["SigmaConvergence"]


class SigmaConvergence:
    """Detect non-converging σ traces (oscillation / stall) without self-reporting."""

    def __init__(
        self,
        gate: Optional[Any] = None,
        *,
        window: int = 5,
        stall_threshold: float = 0.001,
        max_turns: int = 50,
        timeout_s: float = 300.0,
    ) -> None:
        from cos.sigma_gate import SigmaGate

        self.gate = gate if gate is not None else SigmaGate()
        self.window = max(2, int(window))
        self.stall_threshold = float(stall_threshold)
        self.max_turns = int(max_turns)
        self.timeout_s = float(timeout_s)
        self.sigma_trace: List[Dict[str, Any]] = []
        self.start_time: Optional[float] = None

    def begin(self) -> None:
        """Reset trace and start the wall-clock budget for :meth:`check`."""
        self.sigma_trace = []
        self.start_time = time.time()

    def record(self, sigma: float) -> None:
        """Append one σ sample from the current turn (monotonic wall time)."""
        self.sigma_trace.append(
            {
                "σ": float(sigma),
                "timestamp": time.time(),
                "turn": len(self.sigma_trace),
            }
        )

    def check(self) -> Dict[str, Any]:
        """Return a diagnosis for the latest trace (CONTINUE / CONVERGED / LOOP / OSCILLATING / HALT)."""
        n = len(self.sigma_trace)
        if n >= self.max_turns:
            return {"status": "HALT", "reason": "max_turns exceeded", "turns": n}

        t0 = self.start_time if self.start_time is not None else time.time()
        elapsed = time.time() - t0
        if elapsed > self.timeout_s:
            return {"status": "HALT", "reason": "timeout", "elapsed_s": round(elapsed, 1)}

        if n < self.window:
            return {"status": "CONTINUE", "reason": "insufficient data"}

        recent = [float(t["σ"]) for t in self.sigma_trace[-self.window :]]
        delta_sigma = abs(recent[-1] - recent[0]) / float(self.window)

        diffs = [recent[i + 1] - recent[i] for i in range(len(recent) - 1)]
        sign_changes = sum(1 for i in range(len(diffs) - 1) if diffs[i] * diffs[i + 1] < 0)
        oscillating = bool(len(diffs) >= 2 and sign_changes >= len(diffs) - 1)

        current_sigma = recent[-1]

        if delta_sigma < self.stall_threshold and current_sigma > 0.5:
            return {
                "status": "LOOP",
                "reason": "σ stalled at high value",
                "σ": round(current_sigma, 4),
                "Δσ": round(delta_sigma, 6),
                "action": "HALT — switch strategy or ABSTAIN",
            }

        if oscillating and current_sigma > 0.3:
            return {
                "status": "OSCILLATING",
                "reason": "σ bouncing without convergence",
                "σ": round(current_sigma, 4),
                "sign_changes": sign_changes,
                "action": "HALT — deadlock between strategies",
            }

        if delta_sigma < self.stall_threshold and current_sigma < 0.2:
            return {
                "status": "CONVERGED",
                "reason": "σ stable at low value",
                "σ": round(current_sigma, 4),
                "action": "DONE — result is coherent",
            }

        return {
            "status": "CONTINUE",
            "σ": round(current_sigma, 4),
            "Δσ": round(delta_sigma, 6),
            "trend": "improving" if recent[-1] < recent[0] else "degrading",
        }

    def wrap_loop(self, step_fn: Callable[[], float], max_steps: Optional[int] = None) -> List[Dict[str, Any]]:
        """Run ``step_fn`` until σ halts (LOOP / OSCILLATING / CONVERGED / HALT) or step budget exhausted."""
        self.begin()
        lim = int(max_steps) if max_steps is not None else self.max_turns
        results: List[Dict[str, Any]] = []

        for i in range(lim):
            sigma = float(step_fn())
            self.record(sigma)
            diagnosis = self.check()
            results.append({"step": i, "σ": sigma, "diagnosis": diagnosis["status"]})
            if diagnosis["status"] in ("LOOP", "OSCILLATING", "CONVERGED", "HALT"):
                results.append({"final": diagnosis})
                break
        return results
