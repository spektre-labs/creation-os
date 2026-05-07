# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Hierarchical goal hints + dynamic replanning under the σ-gate.

Plan → execute step → :meth:`~cos.sigma_gate.SigmaGate.score` per step → replace
remaining steps (no full restart) when uncertainty triggers replanning.

**NOT AGI ACHIEVED** — lab heuristic decomposition and local replans only; see
``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

from typing import Any, Callable, Dict, List, Optional

from cos.sigma_gate import SigmaGate

__all__ = ["Step", "SigmaPlanner"]


def _planner_verdict_str(verdict: Any) -> str:
    raw = str(getattr(verdict, "name", verdict))
    return raw.split(".")[-1] if "." in raw else raw


class Step:
    """One plan row: optional substeps, gate σ after execution, and status."""

    def __init__(self, description: str, substeps: Optional[List["Step"]] = None) -> None:
        self.description = str(description)
        self.substeps: List[Step] = list(substeps or [])
        self.status = "pending"  # pending | running | done | failed
        self.sigma: Optional[float] = None
        self.result: Any = None


class SigmaPlanner:
    """Decompose goals into steps and execute with σ-gated replanning."""

    def __init__(self, gate: Any = None) -> None:
        self.gate = gate or SigmaGate()
        self.plans: List[Dict[str, Any]] = []
        self.replan_count = 0

    def decompose(self, goal: str, max_depth: int = 3) -> Dict[str, Any]:
        """Decompose *goal* into a versioned plan (new list of :class:`Step`)."""
        steps = self._generate_steps(goal.strip(), max_depth=max(0, int(max_depth)))
        plan: Dict[str, Any] = {"goal": goal, "steps": steps, "version": 0}
        self.plans.append(plan)
        return plan

    def execute(
        self,
        plan: Dict[str, Any],
        execute_fn: Optional[Callable[[str], Any]] = None,
    ) -> Dict[str, Any]:
        """Run each step under the σ-gate; replan remaining tail if uncertainty is high."""
        results: List[Dict[str, Any]] = []
        steps: list = plan["steps"]
        i = 0
        while i < len(steps):
            step: Step = steps[i]
            step.status = "running"

            if execute_fn is not None:
                step.result = execute_fn(step.description)
            else:
                step.result = f"Executed: {step.description}"

            σ, verdict = self.gate.score(str(plan["goal"]), str(step.result))
            vstr = _planner_verdict_str(verdict)
            σf = float(σ)
            step.sigma = round(σf, 4)
            step.status = "failed" if vstr == "ABSTAIN" else "done"

            results.append(
                {
                    "step": step.description,
                    "σ": step.sigma,
                    "verdict": vstr,
                    "status": step.status,
                }
            )

            high_uncertainty = vstr == "ABSTAIN" or (vstr == "RETHINK" and σf > 0.6)
            remaining = steps[i + 1 :]
            if high_uncertainty and remaining:
                new_steps = self._replan(
                    str(plan["goal"]),
                    results,
                    remaining,
                    high_uncertainty=True,
                )
                plan["steps"] = steps[: i + 1] + new_steps
                steps = plan["steps"]
                plan["version"] = int(plan.get("version", 0)) + 1
                self.replan_count += 1
                results.append(
                    {
                        "step": "REPLAN",
                        "reason": f"σ={σf:.3f} verdict={vstr} triggered replanning",
                        "new_steps": len(new_steps),
                    }
                )

            i += 1

        σ_rows = [float(r["σ"]) for r in results if "σ" in r]
        mean_σ = sum(σ_rows) / max(len(σ_rows), 1)
        return {
            "goal": plan["goal"],
            "results": results,
            "replans": self.replan_count,
            "final_σ": round(mean_σ, 4),
            "plan_version": int(plan.get("version", 0)),
        }

    def _generate_steps(self, goal: str, *, max_depth: int) -> List[Step]:
        words = goal.split()
        if len(words) <= 3:
            return [Step(goal)]
        if max_depth <= 1:
            return [Step(f"Solve: {goal}"), Step(f"Verify: {goal}")]
        return [
            Step(f"Understand: {goal}"),
            Step(f"Plan approach for: {goal}"),
            Step(f"Execute: {goal}"),
            Step(f"Verify result of: {goal}"),
        ]

    def _replan(
        self,
        goal: str,
        history: List[Dict[str, Any]],
        remaining: List[Step],
        *,
        high_uncertainty: bool,
    ) -> List[Step]:
        failed = [r for r in history if r.get("status") == "failed"]
        if failed or high_uncertainty:
            return [
                Step(f"Alternative approach for: {goal}"),
                Step(f"Verify alternative: {goal}"),
            ]
        return [Step(s.description) for s in remaining]
