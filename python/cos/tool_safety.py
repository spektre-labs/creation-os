# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tool safety: pattern-based SAFE/MODERATE/BLOCKED plus σ-before-execute, and a 3-tier registry.

Policy classification for :meth:`sigma_before_execute` is **non-probabilistic** (substring rules).
The σ-gate scores proposed tool calls **before** execution. Separately, :meth:`check` implements an
**organizational** 3-tier map (read / σ-mediated / human-required) for agent policy — this is
**lab / product hook** documentation, not EU AI Act or NIST certification.

References (informative): arXiv 2603.20953 (pre-action guardrails), 2604.02375 (agent routing).
**Zero** third-party dependencies beyond :class:`~cos.sigma_gate.SigmaGate`.
"""
from __future__ import annotations

import json
import time
from typing import Any, Callable, Dict, List, Optional

from cos.sigma_gate import SigmaGate

__all__ = [
    "BLOCKED_PATTERNS",
    "MODERATE_PATTERNS",
    "TOOL_TIERS",
    "ToolSafety",
]


TOOL_TIERS: Dict[str, int] = {
    # Tier 1 — read-only / low blast radius (auto-approve in tier policy)
    "read_file": 1,
    "search": 1,
    "list_directory": 1,
    "get_weather": 1,
    "calculate": 1,
    "cos_score": 1,
    # Tier 2 — σ-gate decides (APPROVE / REVIEW / BLOCK)
    "write_file": 2,
    "send_message": 2,
    "create_task": 2,
    "update_database": 2,
    "make_api_call": 2,
    # Tier 3 — human approval required (policy default)
    "delete_file": 3,
    "send_email": 3,
    "transfer_funds": 3,
    "deploy": 3,
    "execute_code": 3,
    "publish": 3,
    "drop_database": 3,
    "terminate_process": 3,
}


BLOCKED_PATTERNS: List[str] = [
    "rm -rf",
    "mkfs",
    "dd if=",
    "shutdown",
    "reboot",
    "DROP TABLE",
    "DROP DATABASE",
    "TRUNCATE",
    "format c:",
    "del /f /s",
]

MODERATE_PATTERNS: List[str] = [
    "rm ",
    "mv ",
    "cp ",
    "chmod",
    "chown",
    "pip install",
    "npm install",
    "apt install",
    "git push",
    "git reset --hard",
    "DELETE FROM",
    "UPDATE ",
    "INSERT INTO",
    "send_email",
    "post_tweet",
    "transfer_funds",
]


def _verdict_str(verdict: object) -> str:
    if hasattr(verdict, "name"):
        return str(getattr(verdict, "name"))
    raw = str(verdict)
    if "." in raw:
        return raw.rsplit(".", 1)[-1]
    return raw


class ToolSafety:
    """Deterministic pattern classifier + σ-before-execute, plus optional 3-tier tool policy."""

    def __init__(
        self,
        gate: Any = None,
        *,
        tiers: Optional[Dict[str, int]] = None,
        custom_blocked: Optional[List[str]] = None,
        custom_moderate: Optional[List[str]] = None,
    ) -> None:
        self.gate = gate or SigmaGate()
        self.blocked: List[str] = list(BLOCKED_PATTERNS) + list(custom_blocked or [])
        self.moderate: List[str] = list(MODERATE_PATTERNS) + list(custom_moderate or [])
        self.audit: List[Dict[str, Any]] = []
        self.tiers: Dict[str, int] = dict(tiers) if tiers is not None else dict(TOOL_TIERS)
        self.tier_log: List[Dict[str, Any]] = []

    def _format_args(self, args: Any) -> str:
        if args is None:
            return ""
        if isinstance(args, (dict, list)):
            return json.dumps(args, sort_keys=True, default=str)
        return str(args)

    def check(self, tool_name: str, args: Any = None, context: str = "") -> Dict[str, Any]:
        """3-tier policy: 1 = auto APPROVE, 2 = σ-gate, 3 = HUMAN_REQUIRED; unknown tools → tier 3."""
        tier = int(self.tiers.get(str(tool_name), 3))
        args_s = self._format_args(args)
        prompt = f"tool call: {tool_name}({args_s})"
        ctx = str(context or "")

        if tier == 1:
            result = {
                "tool": str(tool_name),
                "tier": 1,
                "action": "APPROVE",
                "reason": "safe tool (read-only)",
            }
        elif tier == 2:
            sigma, verdict = self.gate.score(prompt, ctx or f"policy context for {tool_name}")
            vn = _verdict_str(verdict)
            sig_r = round(float(sigma), 4)
            if vn == "ACCEPT":
                result = {"tool": str(tool_name), "tier": 2, "action": "APPROVE", "sigma": sig_r}
            elif vn == "RETHINK":
                result = {
                    "tool": str(tool_name),
                    "tier": 2,
                    "action": "REVIEW",
                    "sigma": sig_r,
                    "reason": "σ uncertain — review recommended",
                }
            else:
                result = {
                    "tool": str(tool_name),
                    "tier": 2,
                    "action": "BLOCK",
                    "sigma": sig_r,
                    "reason": "σ too high",
                }
        else:
            result = {
                "tool": str(tool_name),
                "tier": 3,
                "action": "HUMAN_REQUIRED",
                "reason": "irreversible action — human must approve",
            }

        self.tier_log.append(result)
        return result

    def register_tool(self, name: str, tier: int) -> None:
        if tier not in (1, 2, 3):
            raise ValueError("Tier must be 1, 2, or 3")
        self.tiers[str(name)] = int(tier)

    def audit_log(self) -> Dict[str, Any]:
        """Aggregates for tier-policy checks (append-only list in memory; persist externally if needed)."""
        log = list(self.tier_log)
        return {
            "total_checks": len(log),
            "approved": sum(1 for row in log if row.get("action") == "APPROVE"),
            "blocked": sum(1 for row in log if row.get("action") == "BLOCK"),
            "review": sum(1 for row in log if row.get("action") == "REVIEW"),
            "human_required": sum(1 for row in log if row.get("action") == "HUMAN_REQUIRED"),
            "log": log,
        }

    def classify(self, tool_name: str, args_str: str = "") -> str:
        """Classify a tool call → SAFE / MODERATE / BLOCKED."""
        combined = f"{tool_name} {args_str}".lower()
        for pattern in self.blocked:
            if pattern.lower() in combined:
                return "BLOCKED"
        for pattern in self.moderate:
            if pattern.lower() in combined:
                return "MODERATE"
        return "SAFE"

    def sigma_before_execute(self, tool_name: str, args_str: str = "", intent: str = "") -> Dict[str, Any]:
        """σ-gate **before** execution. Returns classification, σ, verdict, allow, reason."""
        classification = self.classify(tool_name, args_str)

        if classification == "BLOCKED":
            reason = f"Blocked pattern in: {tool_name} {args_str}"
            self._log(tool_name, args_str, classification, 1.0, "ABSTAIN", False)
            return {
                "classification": "BLOCKED",
                "sigma": 1.0,
                "verdict": "ABSTAIN",
                "allow": False,
                "reason": reason,
            }

        prompt = f"Tool call: {tool_name}({args_str})"
        response = intent or f"Executing {tool_name} with {args_str}"
        sigma, verdict = self.gate.score(prompt, response)
        vn = _verdict_str(verdict)

        if classification == "MODERATE":
            allow = vn == "ACCEPT"
            reason = "MODERATE: σ low, auto-approved" if allow else "MODERATE: requires approval"
        else:
            allow = vn != "ABSTAIN"
            reason = "SAFE" if allow else "SAFE but σ too high"

        self._log(tool_name, args_str, classification, float(sigma), vn, allow)
        return {
            "classification": classification,
            "sigma": float(sigma),
            "verdict": vn,
            "allow": allow,
            "reason": reason,
        }

    def approval_gate(
        self,
        tool_name: str,
        args_str: str = "",
        *,
        user_callback: Optional[Callable[[str, str, Dict[str, Any]], bool]] = None,
    ) -> Dict[str, Any]:
        """Human confirmation path for MODERATE tool calls that σ did not ACCEPT."""
        result = self.sigma_before_execute(tool_name, args_str)
        if (
            result["classification"] == "MODERATE"
            and not result["allow"]
            and user_callback is not None
        ):
            approved = bool(user_callback(tool_name, args_str, result))
            result["allow"] = approved
            result["reason"] += f" → user {'approved' if approved else 'rejected'}"
        return result

    def get_audit_log(self) -> List[Dict[str, Any]]:
        return list(self.audit)

    def _log(
        self,
        tool: str,
        args: str,
        classification: str,
        sigma: float,
        verdict: str,
        allow: bool,
    ) -> None:
        self.audit.append(
            {
                "timestamp": time.time(),
                "tool": tool,
                "args": args,
                "classification": classification,
                "sigma": round(float(sigma), 4),
                "verdict": verdict,
                "allow": allow,
            }
        )


# Thoth-style public alias (Unicode σ)
ToolSafety.σ_before_execute = ToolSafety.sigma_before_execute  # type: ignore[attr-defined]

