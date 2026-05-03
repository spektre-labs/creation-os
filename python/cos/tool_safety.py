# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tool safety: SAFE / MODERATE / BLOCKED plus σ-before-execute (deterministic policy first).

Policy classification is **non-probabilistic** (substring rules). The σ-gate then scores
the proposed tool call **before** execution: BLOCKED calls never reach the gate output for
execution (immediate ABSTAIN envelope). MODERATE calls require ACCEPT from σ unless a
human ``approval_gate`` callback overrides.

References (informative): arXiv 2603.20953 (pre-action guardrails), 2604.02375 (agent routing).
**Zero** third-party dependencies beyond :class:`~cos.sigma_gate.SigmaGate`.
"""
from __future__ import annotations

import time
from typing import Any, Callable, Dict, List, Optional

from cos.sigma_gate import SigmaGate

__all__ = [
    "BLOCKED_PATTERNS",
    "MODERATE_PATTERNS",
    "ToolSafety",
]


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
    """Deterministic tool-call classifier + σ gate prior to execution."""

    def __init__(
        self,
        gate: Any = None,
        *,
        custom_blocked: Optional[List[str]] = None,
        custom_moderate: Optional[List[str]] = None,
    ) -> None:
        self.gate = gate or SigmaGate()
        self.blocked: List[str] = list(BLOCKED_PATTERNS) + list(custom_blocked or [])
        self.moderate: List[str] = list(MODERATE_PATTERNS) + list(custom_moderate or [])
        self.audit: List[Dict[str, Any]] = []

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

