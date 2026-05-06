# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
σ-agent — tool-use runtime with pre-tool and post-tool σ, plus legacy planner agent.

v140 ``SigmaAgent`` scores tool calls and final answers with the same σ facade used
elsewhere (``gate.score(prompt, response) -> (float, Verdict)``).

Does not modify ``sigma_gate.h``. Lab / harness only unless wired to measured gates;
see ``docs/CLAIM_DISCIPLINE.md``.
"""
from __future__ import annotations

import json
from dataclasses import dataclass
from typing import Any, Dict, List, Optional, Protocol, Tuple, Union, runtime_checkable

from .sigma_gate_core import Verdict


@dataclass
class AgentAction:
    """Planner-facing tool invocation (legacy ``SigmaPlannerAgent``)."""

    tool_name: str
    params: Dict[str, Any]


def _verdict_name(v: Union[str, Verdict]) -> str:
    if isinstance(v, Verdict):
        return v.name
    s = str(v).strip().upper()
    for name in ("ACCEPT", "RETHINK", "ABSTAIN"):
        if name in s:
            return name
    return "RETHINK"


def _sigma_to_verdict_label(sigma: float) -> str:
    s = float(sigma)
    if s < 0.3:
        return Verdict.ACCEPT.name
    if s < 0.7:
        return Verdict.RETHINK.name
    return Verdict.ABSTAIN.name


class SigmaTool:
    """Tool base with lightweight JSON-schema-style args checks (required keys only)."""

    def __init__(
        self,
        name: str,
        description: str,
        schema: Dict[str, Any],
        *,
        is_destructive: bool = False,
    ) -> None:
        self.name = str(name)
        self.description = str(description)
        self.schema = dict(schema)
        self.is_destructive = bool(is_destructive)
        self.confirmed = False

    def confirm(self) -> None:
        self.confirmed = True

    def validate_args(self, args: Any) -> bool:
        if not isinstance(args, dict):
            return False
        required = self.schema.get("required")
        if isinstance(required, list):
            for key in required:
                if key not in args:
                    return False
        return True

    def run(self, **kwargs: Any) -> Any:
        if not kwargs and not self.schema.get("properties"):
            return {"ok": True, "tool": self.name, "echo": "no-arg lab stub"}
        return {
            "ok": False,
            "tool": self.name,
            "error": "subclass_or_register_implementation",
            "received_keys": sorted(kwargs.keys()),
        }


@runtime_checkable
class ToolLoopGate(Protocol):
    def score(self, prompt: str, text: str) -> Tuple[float, Verdict]:
        ...


@runtime_checkable
class ToolLoopModel(Protocol):
    def generate(self, context: List[Dict[str, Any]], tools: List[SigmaTool]) -> Dict[str, Any]:
        ...


class SigmaAgent:
    """σ-gated tool loop: pre-tool σ, execute, post-tool σ, then final answer σ."""

    def __init__(
        self,
        model: ToolLoopModel,
        gate: ToolLoopGate,
        tools: List[SigmaTool],
        *,
        max_steps: int = 20,
    ) -> None:
        self.model = model
        self.gate = gate
        self.tools = {t.name: t for t in tools}
        self.max_steps = int(max_steps)
        self.trace: List[Dict[str, Any]] = []

    def run(self, goal: str) -> Dict[str, Any]:
        context: List[Dict[str, Any]] = [{"role": "user", "content": goal}]
        self.trace = []
        tool_list = list(self.tools.values())

        for step in range(self.max_steps):
            response = self.model.generate(context, tool_list)
            if not isinstance(response, dict):
                return self._fail(step, "invalid_model_response")

            if response.get("tool_call"):
                tc = response["tool_call"]
                if not isinstance(tc, dict):
                    return self._fail(step, "invalid_tool_call")
                pre = self.pre_tool_check(tc, context)
                pv = str(pre.get("verdict", "")).upper()
                if pv == Verdict.ABSTAIN.name:
                    self.trace.append(
                        {
                            "step": step,
                            "action": "tool_blocked",
                            "reason": pre.get("reason") or f"pre-tool σ={float(pre.get('sigma', 1.0)):.3f}",
                            "pre_sigma": float(pre.get("sigma", 1.0)),
                        }
                    )
                    context.append(
                        {
                            "role": "system",
                            "content": "Tool call blocked by σ-gate. Try a different approach.",
                        }
                    )
                    continue
                if pv == Verdict.RETHINK.name:
                    self.trace.append(
                        {
                            "step": step,
                            "action": "tool_rethink",
                            "tool": tc.get("name"),
                            "reason": pre.get("reason", "pre_tool_rethink"),
                            "pre_sigma": float(pre.get("sigma", 0.8)),
                        }
                    )
                    context.append(
                        {
                            "role": "system",
                            "content": "σ-gate: RETHINK this tool plan (e.g. confirm destructive action).",
                        }
                    )
                    continue

                tool_result = self.execute_tool(tc)
                post = self.post_tool_check(tc, tool_result)
                post_v = _verdict_name(post.get("verdict", Verdict.RETHINK))
                self.trace.append(
                    {
                        "step": step,
                        "action": "tool_call",
                        "tool": tc.get("name"),
                        "pre_sigma": float(pre.get("sigma", 0.0)),
                        "post_sigma": float(post.get("sigma", 0.0)),
                        "verdict": post_v,
                    }
                )
                context.append({"role": "tool", "content": json.dumps(tool_result, default=str)})

            else:
                answer = str(response.get("content", "") or "")
                sigma, verdict = self.gate.score(goal, answer)
                vn = _verdict_name(verdict)
                self.trace.append(
                    {
                        "step": step,
                        "action": "answer",
                        "sigma": float(sigma),
                        "verdict": vn,
                    }
                )
                if vn == Verdict.ACCEPT.name:
                    return {
                        "answer": answer,
                        "sigma": float(sigma),
                        "verdict": vn,
                        "steps": step + 1,
                        "trace": list(self.trace),
                    }
                if vn == Verdict.RETHINK.name:
                    context.append(
                        {
                            "role": "system",
                            "content": "σ-gate: RETHINK. Your answer may not be reliable. Try again.",
                        }
                    )
                    continue
                return {
                    "answer": "I cannot provide a reliable answer.",
                    "sigma": float(sigma),
                    "verdict": Verdict.ABSTAIN.name,
                    "steps": step + 1,
                    "trace": list(self.trace),
                }

        return {
            "answer": None,
            "sigma": 1.0,
            "verdict": "MAX_STEPS",
            "steps": self.max_steps,
            "trace": list(self.trace),
        }

    def _fail(self, step: int, reason: str) -> Dict[str, Any]:
        self.trace.append({"step": step, "action": "error", "reason": reason})
        return {
            "answer": None,
            "sigma": 1.0,
            "verdict": "ABSTAIN",
            "steps": step + 1,
            "trace": list(self.trace),
        }

    def pre_tool_check(self, tool_call: Dict[str, Any], _context: List[Dict[str, Any]]) -> Dict[str, Any]:
        tool_name = str(tool_call.get("name", "") or "")
        args = tool_call.get("arguments", {})
        if tool_name not in self.tools:
            return {"sigma": 1.0, "verdict": Verdict.ABSTAIN.name, "reason": "unknown tool"}
        tool = self.tools[tool_name]
        if not tool.validate_args(args):
            return {"sigma": 0.9, "verdict": Verdict.ABSTAIN.name, "reason": "invalid args"}
        if tool.is_destructive and not tool.confirmed:
            return {
                "sigma": 0.8,
                "verdict": Verdict.RETHINK.name,
                "reason": "destructive unconfirmed",
            }

        prompt = f"Should I call {tool_name} with {json.dumps(args, sort_keys=True)}?"
        response = f"Yes, calling {tool_name} is appropriate in this context."
        sigma, verdict = self.gate.score(prompt, response)
        return {"sigma": float(sigma), "verdict": _verdict_name(verdict)}

    def post_tool_check(self, tool_call: Dict[str, Any], result: Any) -> Dict[str, Any]:
        payload = json.dumps(result, default=str)
        if len(payload) > 500:
            payload = payload[:500] + "…"
        prompt = f"Tool {tool_call.get('name')} was called."
        sigma, verdict = self.gate.score(prompt, payload)
        return {"sigma": float(sigma), "verdict": _verdict_name(verdict)}

    def execute_tool(self, tool_call: Dict[str, Any]) -> Any:
        name = str(tool_call.get("name", "") or "")
        tool = self.tools[name]
        args = tool_call.get("arguments")
        if not isinstance(args, dict):
            args = {}
        try:
            return tool.run(**args)
        except Exception as e:
            return {"error": str(e)}


@runtime_checkable
class PlannerGate(Protocol):
    def compute_sigma(self, model: Any, goal: str, action: AgentAction) -> float:
        ...


@runtime_checkable
class PlannerModel(Protocol):
    def plan(self, goal: str, history: List[Dict[str, Any]]) -> AgentAction:
        ...

    def replan(self, goal: str, history: List[Dict[str, Any]], hint: str) -> AgentAction:
        ...


class SigmaPlannerAgent:
    """Legacy planner agent (``plan`` / ``compute_sigma``) used by ``cos agent --legacy``."""

    def __init__(
        self,
        model: PlannerModel,
        gate: PlannerGate,
        tools: Dict[str, Any],
        *,
        allow_destructive: bool = False,
        audit: Optional[Any] = None,
    ) -> None:
        self.model = model
        self.gate = gate
        self.tools = tools
        self.allow_destructive = bool(allow_destructive)
        self.audit = audit

    def run(self, goal: str, max_steps: int = 10) -> List[Dict[str, Any]]:
        history: List[Dict[str, Any]] = []
        h: List[Dict[str, Any]] = []
        for step in range(max_steps):
            action = self.model.plan(goal, h)
            sigma = float(self.gate.compute_sigma(self.model, goal, action))
            verdict = _sigma_to_verdict_label(sigma)

            destructive = action.tool_name in ("rm", "delete") or "rm -rf" in str(action.params).lower()
            result: Any = None
            if destructive and not self.allow_destructive:
                verdict = Verdict.ABSTAIN.name
                result = {"blocked": True, "reason": "destructive_not_allowed"}
            elif verdict == Verdict.ACCEPT.name and action.tool_name in self.tools:
                tool = self.tools[action.tool_name]
                result = tool.run(action.params)

            row = {"step": step, "action": action, "sigma": sigma, "verdict": verdict, "result": result}
            history.append(row)
            if self.audit is not None and hasattr(self.audit, "log_agent_step"):
                self.audit.log_agent_step(
                    {
                        "step": step,
                        "tool": action.tool_name,
                        "params": action.params,
                        "sigma": sigma,
                        "verdict": verdict,
                    }
                )
            h.append({"tool": action.tool_name, "params": action.params, "result": result, "verdict": verdict})
        return history


__all__ = [
    "AgentAction",
    "SigmaAgent",
    "SigmaPlannerAgent",
    "SigmaTool",
]

