# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
v174 σ-planning: hierarchical text plans with σ risk surfacing (lab).

**Not MPC / STRIPS:** no formal action model; outputs are prose JSON-friendly dicts.
"""
from __future__ import annotations

import statistics
from typing import Any, Dict, List, Optional


class SigmaPlanner:
    def __init__(self, gate: Any, world_model: Any, model: Any) -> None:
        self.gate = gate
        self.world = world_model
        self.model = model

    @staticmethod
    def parse_steps(text: str) -> List[str]:
        lines = [ln.strip().lstrip("-*0123456789.) ") for ln in text.splitlines() if ln.strip()]
        return lines[:16] or [text.strip()]

    def plan(self, goal: str, constraints: Optional[str] = None, max_depth: int = 3) -> Dict[str, Any]:
        _ = constraints
        abstract_plan = self.plan_abstract(goal)
        concrete_plan = self.refine(abstract_plan, depth=0, max_depth=int(max_depth))
        risks = self.analyze_risks(concrete_plan)
        fallbacks = self.generate_fallbacks(concrete_plan, risks)
        return {
            "goal": goal,
            "plan": concrete_plan,
            "risks": risks,
            "fallbacks": fallbacks,
            "overall_sigma": float(self.plan_sigma(concrete_plan)),
        }

    def plan_abstract(self, goal: str) -> Dict[str, Any]:
        response = self.model.generate(f"Create a high-level plan (3-5 phases) to achieve: {goal}")
        sigma, verdict = self.gate.score(goal, response)
        return {"level": "abstract", "steps": self.parse_steps(response), "sigma": float(sigma), "verdict": verdict}

    def refine(self, plan: Dict[str, Any], depth: int, max_depth: int) -> Dict[str, Any]:
        if depth >= max_depth:
            return plan
        refined_steps: List[Dict[str, Any]] = []
        for step in plan.get("steps", []):
            if isinstance(step, dict):
                refined_steps.append(step)
                continue
            substeps_text = self.model.generate(f"Break down into concrete actions: {step}")
            sigma, _ = self.gate.score(str(step), substeps_text)
            refined_steps.append(
                {
                    "step": step,
                    "substeps": self.parse_steps(substeps_text),
                    "sigma": float(sigma),
                    "level": f"detail_{depth + 1}",
                }
            )
        plan = dict(plan)
        plan["steps"] = refined_steps
        return plan

    def analyze_risks(self, plan: Dict[str, Any]) -> List[Dict[str, Any]]:
        risks: List[Dict[str, Any]] = []
        steps = plan.get("steps", [])
        for step in steps:
            label = step.get("step", step) if isinstance(step, dict) else step
            risk = self.model.generate(f"What could go wrong with: {label}?")
            sigma, _ = self.gate.score(str(label), risk)
            if float(sigma) < 0.5:
                risks.append({"step": step, "risk": risk, "sigma": float(sigma)})
        return risks

    def generate_fallbacks(self, plan: Dict[str, Any], risks: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
        _ = plan
        fallbacks: List[Dict[str, Any]] = []
        for risk in risks:
            fallback = self.model.generate(f"If this happens: {risk['risk']}, what is the backup plan?")
            sigma, _ = self.gate.score(risk["risk"], fallback)
            fallbacks.append({"risk": risk, "fallback": fallback, "sigma": float(sigma)})
        return fallbacks

    def plan_sigma(self, plan: Dict[str, Any]) -> float:
        sigs: List[float] = []
        for step in plan.get("steps", []):
            if isinstance(step, dict) and "sigma" in step:
                sigs.append(float(step["sigma"]))
            elif isinstance(plan, dict) and "sigma" in plan:
                sigs.append(float(plan["sigma"]))
        if not sigs and "sigma" in plan:
            sigs.append(float(plan["sigma"]))
        return statistics.fmean(sigs) if sigs else 0.5


__all__ = ["SigmaPlanner"]
