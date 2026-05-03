# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-MCP v3 — lab protocol shim (capabilities, transports, σ per primitive).

This module is a **portable orchestration layer** around :class:`~cos.sigma_gate.SigmaGate`
and :class:`~cos.agent_guard.SigmaAgentGuard`. It does not embed the optional ``mcp`` SDK;
for FastMCP tools see :mod:`cos.mcp_sigma_server`. Transport servers (stdio / streamable HTTP)
are out of scope here—configure them in your host process.

See ``docs/CLAIM_DISCIPLINE.md``. **NOT AGI** / not full third-party MCP compliance.
"""
from __future__ import annotations

import json
import time
from pathlib import Path
from typing import Any, Dict, List, Optional, Set
from urllib.parse import urlparse

__all__ = ["SigmaA2AAgent", "SigmaMCPServer"]

# Negotiated protocol revision label (spec family; bump with host integration).
PROTOCOL_VERSION = "2025-11-25"


class SigmaMCPServer:
    """MCP-shaped primitives + σ-gating + trust firewall (lab)."""

    PRIMITIVES: tuple[str, ...] = ("tools", "resources", "prompts", "sampling", "roots")

    def __init__(
        self,
        gate: Any = None,
        *,
        agent_guard: Any = None,
        transport: str = "stdio",
    ) -> None:
        from cos.agent_guard import SigmaAgentGuard
        from cos.sigma_gate import SigmaGate

        self.gate = gate or SigmaGate()
        self.agent_guard = agent_guard or SigmaAgentGuard(self.gate)
        self.transport = str(transport)
        self.oauth_lab_note = (
            "OAuth 2.1 remote auth: configure at transport host; not enforced in stdlib lab."
        )
        self._blocked_methods: Set[str] = set()
        self._calls_per_bucket: Dict[str, int] = {}
        self.rpm_limit_per_key: int = 600
        self.audit_log: List[Dict[str, Any]] = []

    def set_transport(self, name: str) -> None:
        """``stdio`` (local) or ``streamable_http`` (remote; host must implement HTTP)."""
        n = str(name).lower().replace("-", "_")
        if n == "streamablehttp":
            n = "streamable_http"
        if n not in ("stdio", "streamable_http"):
            raise ValueError(f"unknown MCP transport {name!r}")
        self.transport = n

    def capability_negotiation(
        self,
        client_caps: Optional[Dict[str, Any]] = None,
    ) -> Dict[str, Any]:
        """initialize → capabilities → ready-shaped bundle (JSON-serializable)."""
        _ = client_caps
        return {
            "protocolVersion": PROTOCOL_VERSION,
            "capabilities": {
                "tools": {"listChanged": False},
                "resources": {"subscribe": False},
                "prompts": {},
                "sampling": {},
                "roots": {"listChanged": False},
            },
            "serverInfo": {"name": "creation-os-sigma-mcp-lab", "version": PROTOCOL_VERSION},
            "oauth": {"mode": "optional", "note": self.oauth_lab_note},
            "ready": True,
        }

    def tool_annotations(
        self,
        tool_name: str,
        *,
        read_only_hint: bool = False,
        destructive_hint: bool = False,
        idempotent_hint: bool = False,
    ) -> Dict[str, Any]:
        """Map MCP tool hints → agent_guard reversibility class (lab matrix)."""
        if destructive_hint:
            rev = "irreversible"
        elif read_only_hint:
            rev = "reversible"
        elif idempotent_hint:
            rev = "reversible"
        else:
            ac = self.agent_guard.action_classify(
                {"tool": tool_name, "args": {}},
            )
            rev = self.agent_guard.reversibility(ac)
        return {
            "tool": str(tool_name),
            "readOnlyHint": bool(read_only_hint),
            "destructiveHint": bool(destructive_hint),
            "idempotentHint": bool(idempotent_hint),
            "reversibility": rev,
        }

    def score_tool_call(self, tool_name: str, args: Any) -> Dict[str, Any]:
        ann = self.tool_annotations(
            tool_name,
            read_only_hint="read" in str(tool_name).lower(),
        )
        scored = self.agent_guard.score_tool_call(str(tool_name), args, gate=self.gate)
        out = {**scored, "annotations": ann}
        self.audit_log.append({"primitive": "tools", "payload": out})
        return out

    def score_resource_read(self, resource_uri: str) -> Dict[str, Any]:
        uri = str(resource_uri)
        sigma, verdict = self.gate.score("read", uri)
        row = {
            "resource_uri": uri,
            "sigma": round(float(sigma), 6),
            "verdict": str(verdict),
        }
        self.audit_log.append({"primitive": "resources", "payload": row})
        return row

    def score_sampling(self, prompt: str, response: str) -> Dict[str, Any]:
        sigma, verdict = self.gate.score(str(prompt), str(response))
        row = {
            "sigma": round(float(sigma), 6),
            "verdict": str(verdict),
        }
        self.audit_log.append({"primitive": "sampling", "payload": row})
        return row

    def trust_firewall_check(
        self,
        method: str,
        *,
        client_id: str = "default",
    ) -> Dict[str, Any]:
        """Rate limit + blocked JSON-RPC-style method names (lab)."""
        m = str(method)
        if m in self._blocked_methods:
            row = {"allow": False, "reason": "blocked_method", "method": m}
            self.audit_log.append({"trust_firewall": row})
            return row
        bucket = int(time.time() // 60)
        key = f"{client_id}:{m}:{bucket}"
        self._calls_per_bucket[key] = self._calls_per_bucket.get(key, 0) + 1
        if self._calls_per_bucket[key] > self.rpm_limit_per_key:
            row = {"allow": False, "reason": "rate_limited", "method": m}
            self.audit_log.append({"trust_firewall": row})
            return row
        return {"allow": True, "reason": "ok", "method": m}

    def block_method(self, method: str) -> None:
        self._blocked_methods.add(str(method))

    def primitives_manifest(self) -> Dict[str, Any]:
        return {
            "protocolVersion": PROTOCOL_VERSION,
            "primitives": list(self.PRIMITIVES),
            "transport": self.transport,
        }


class SigmaA2AAgent:
    """Agent Card discovery + σ-gated delegation (A2A-shaped lab)."""

    AGENT_CARD_PATH = "/.well-known/agent.json"

    def __init__(self, gate: Any = None) -> None:
        from cos.sigma_gate import SigmaGate

        self.gate = gate or SigmaGate()
        self.audit_log: List[Dict[str, Any]] = []

    def discover(self, url: str) -> Dict[str, Any]:
        """Fetch JSON Agent Card (``file:``, ``http(s):``)."""
        p = urlparse(str(url))
        if p.scheme == "file":
            raw = Path(p.path).expanduser().read_text(encoding="utf-8")
            return json.loads(raw)
        if p.scheme in ("http", "https"):
            try:
                import urllib.error
                import urllib.request

                with urllib.request.urlopen(str(url), timeout=5) as resp:
                    body = resp.read().decode("utf-8")
                return json.loads(body)
            except (urllib.error.URLError, json.JSONDecodeError, OSError) as e:
                return {"error": str(e), "agent_card": None}
        return {"error": "unsupported_scheme", "agent_card": None}

    def delegate(self, agent_id: str, task: str) -> Dict[str, Any]:
        blob = f"delegate:{agent_id}\n{task}"
        sigma, verdict = self.gate.score("a2a_delegate", blob[:4000])
        row = {
            "agent_id": str(agent_id),
            "sigma": round(float(sigma), 6),
            "verdict": str(verdict),
            "allowed": str(verdict) == "ACCEPT",
        }
        self.audit_log.append(row)
        return row

    def receive_task(self, task: str) -> Dict[str, Any]:
        sigma, verdict = self.gate.score("a2a_incoming", str(task)[:4000])
        row = {
            "sigma": round(float(sigma), 6),
            "verdict": str(verdict),
            "accept": str(verdict) != "ABSTAIN",
        }
        self.audit_log.append(row)
        return row
