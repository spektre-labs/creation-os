# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Prompt-injection and tool-allowlist guard for MCP-shaped tool calls (lab).

Fast path: substring heuristics. Slow path: σ from :class:`cos.sigma_gate.SigmaGate` or
:func:`cos.sigma_gate_quickscore.quickscore_sigma` when no gate is configured.
"""
from __future__ import annotations

import json
from typing import Any, Dict, List, Optional, Set

_STATS: Dict[str, int] = {"blocked": 0, "passed": 0, "injection_attempts": 0}


def firewall_stats() -> Dict[str, Any]:
    return dict(_STATS)


def reset_firewall_stats() -> None:
    _STATS.clear()
    _STATS.update({"blocked": 0, "passed": 0, "injection_attempts": 0})


class SigmaMCPFirewall:
    INJECTION_PATTERNS: List[str] = [
        "ignore previous instructions",
        "you are now",
        "disregard all",
        "system prompt",
        "override",
        "jailbreak",
    ]

    def __init__(
        self,
        gate: Any = None,
        *,
        allow_tools: Optional[Set[str]] = None,
        sigma_block: float = 0.8,
    ) -> None:
        self.gate = gate
        self.allow_tools: Set[str] = set(allow_tools) if allow_tools else set()
        self.sigma_block = float(sigma_block)

    def is_tool_allowed(self, name: Optional[str]) -> bool:
        if not self.allow_tools:
            return True
        return str(name or "") in self.allow_tools

    def check_incoming(self, tool_call: Dict[str, Any]) -> Dict[str, Any]:
        """Inspect ``{"name": str, "arguments": str|dict}`` style tool calls."""
        name = tool_call.get("name")
        raw = tool_call.get("arguments", {})
        if isinstance(raw, dict):
            content = json.dumps(raw, sort_keys=True, default=str).lower()
        else:
            content = str(raw).lower()

        for pattern in self.INJECTION_PATTERNS:
            if pattern in content:
                _STATS["blocked"] += 1
                _STATS["injection_attempts"] += 1
                return {"blocked": True, "reason": f"injection pattern: {pattern}"}

        sigma = 0.0
        if self.gate is not None and hasattr(self.gate, "score"):
            sigma, _ver = self.gate.score("", content)
        else:
            from cos.sigma_gate_quickscore import quickscore_sigma

            sigma = float(quickscore_sigma("", content))

        if sigma > self.sigma_block:
            _STATS["blocked"] += 1
            return {"blocked": True, "reason": f"high sigma: {sigma:.4f}"}

        if not self.is_tool_allowed(str(name) if name is not None else ""):
            _STATS["blocked"] += 1
            return {"blocked": True, "reason": "tool not in allowlist"}

        r: Dict[str, Any] = {"blocked": False, "sigma": float(sigma)}
        _STATS["passed"] += 1
        return r


__all__ = ["SigmaMCPFirewall", "firewall_stats", "reset_firewall_stats"]
