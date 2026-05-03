# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-agent-guard — lab policy layer on tool calls using ``SigmaGate`` + reversibility.

Maps gate verdicts to EXECUTE / CONFIRM / SIMULATE / BLOCK. This is **not** NeMo/Colang;
no external agent framework. See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import json
from typing import Any, Dict, List, Optional, Set

__all__ = ["SigmaAgentGuard"]


class SigmaAgentGuard:
    """Tool-call screening: σ score, allowlists, budgets, reversible vs irreversible acts."""

    def __init__(self, gate: Any = None) -> None:
        from cos.sigma_gate import SigmaGate

        self.gate = gate or SigmaGate()
        self.audit_log: List[Dict[str, Any]] = []

    def score_tool_call(
        self,
        tool_name: str,
        args: Any,
        gate: Optional[Any] = None,
    ) -> Dict[str, Any]:
        g = gate or self.gate
        try:
            arg_s = json.dumps(args, sort_keys=True, default=str)
        except TypeError:
            arg_s = str(args)
        payload = f"{tool_name}\n{arg_s}"
        sigma, verdict = g.score("tool_call", payload)
        sigma = float(sigma)
        verdict = str(verdict)
        tc = {"tool": str(tool_name), "args": args}
        action = self.action_classify(tc)
        rev = self.reversibility(action)
        decision = self.decide(verdict, rev)
        row = {
            "tool": tool_name,
            "sigma": round(sigma, 6),
            "verdict": verdict,
            "action": action,
            "reversibility": rev,
            "decision": decision,
        }
        self.audit_log.append(dict(row))
        return row

    def action_classify(self, tool_call: Dict[str, Any]) -> str:
        name = str(tool_call.get("tool", "")).lower()
        arg_blob = json.dumps(tool_call.get("args", {}), default=str).lower()

        if any(k in name for k in ("delete", "remove", "drop", "wipe")):
            return "delete"
        if any(k in name for k in ("write", "update", "insert", "patch", "put", "post")):
            if any(k in name for k in ("http", "api", "webhook", "curl")):
                return "external"
            return "write"
        if any(
            k in name
            for k in (
                "run",
                "exec",
                "shell",
                "eval",
                "code",
                "bash",
                "python",
                "npm",
            )
        ):
            return "execute"
        if any(k in name for k in ("http", "fetch", "request", "webhook")):
            return "external"
        if any(k in arg_blob for k in ("rm -", "sudo", "drop table", "__import__")):
            return "execute"
        if any(k in name for k in ("read", "get", "list", "fetch_schema", "query_select")):
            return "read"
        return "read"

    def reversibility(self, action_type: str) -> str:
        at = str(action_type).lower()
        if at in ("read",):
            return "reversible"
        if at in ("write", "delete", "execute", "external"):
            return "irreversible"
        return "unknown"

    def policy_check(
        self,
        tool_call: Dict[str, Any],
        allowed_tools: Optional[Set[str]] = None,
        denied_tools: Optional[Set[str]] = None,
    ) -> Dict[str, Any]:
        name = str(tool_call.get("tool", ""))
        if denied_tools and name in denied_tools:
            return {"allow": False, "reason": "denied_tool"}
        if allowed_tools is not None and name not in allowed_tools:
            return {"allow": False, "reason": "not_in_allowed_set"}
        return {"allow": True, "reason": "ok"}

    def budget_check(
        self,
        tool_call: Dict[str, Any],
        remaining_budget: float,
        *,
        estimated_cost: float = 1.0,
    ) -> Dict[str, Any]:
        cost = float(estimated_cost)
        if cost > float(remaining_budget):
            return {"allow": False, "reason": "budget_exceeded"}
        return {"allow": True, "reason": "ok", "cost": cost}

    def decide(self, verdict: str, reversibility_class: str) -> str:
        v = str(verdict).upper()
        r = str(reversibility_class).lower()
        if v == "ABSTAIN":
            return "BLOCK"
        if v == "RETHINK":
            return "SIMULATE"
        if v == "ACCEPT":
            if r == "irreversible":
                return "CONFIRM"
            if r == "unknown":
                return "CONFIRM"
            return "EXECUTE"
        return "BLOCK"

    def run_guardrails(
        self,
        tool_name: str,
        args: Any,
        *,
        allowed_tools: Optional[Set[str]] = None,
        denied_tools: Optional[Set[str]] = None,
        remaining_budget: Optional[float] = None,
        gate: Optional[Any] = None,
    ) -> Dict[str, Any]:
        tc = {"tool": tool_name, "args": args}
        pol = self.policy_check(tc, allowed_tools, denied_tools)
        if not pol["allow"]:
            self.audit_log.append({"tool": tool_name, "decision": "BLOCK", "reason": pol["reason"]})
            return {"ok": False, "stage": "policy", **pol}
        if remaining_budget is not None:
            bud = self.budget_check(tc, remaining_budget)
            if not bud["allow"]:
                self.audit_log.append(
                    {"tool": tool_name, "decision": "BLOCK", "reason": bud["reason"]},
                )
                return {"ok": False, "stage": "budget", **bud}
        scored = self.score_tool_call(tool_name, args, gate=gate)
        return {"ok": True, "stage": "scored", **scored}
