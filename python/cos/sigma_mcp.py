# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
σ-MCP v2 — JSON-RPC-shaped lab handler + trust firewall + A2A Agent Card helpers.

**Not a full MCP SDK:** hosts should still use FastMCP/stdio for production tool streams
(see :mod:`cos.sigma_mcp_server`). This module is for tests, ``cos mcp --call``, and
embeddings that want a single ``handle_request`` entrypoint.

``PROTOCOL_VERSION`` is a **lab label** — wire to your host's negotiated version string.
"""
from __future__ import annotations

import json
import urllib.error
import urllib.request
from typing import Any, Callable, Dict, List, Optional

# Lab protocol label (do not merge with host/SDK version receipts as a product claim).
PROTOCOL_VERSION = "2026-lab"

ToolsDict = Dict[str, Dict[str, Any]]


class SigmaTrustFirewall:
    """Filter JSON-RPC MCP-style calls before routing (rate limits + block list)."""

    def __init__(self) -> None:
        self.blocked_methods: set = set()
        self.rate_limits: Dict[str, int] = {}
        self.call_log: List[Dict[str, Any]] = []

    def block_method(self, method: str) -> None:
        self.blocked_methods.add(str(method))

    def set_rate_limit(self, method: str, max_per_window: int) -> None:
        self.rate_limits[str(method)] = int(max_per_window)

    def check(self, method: Optional[str], params: Any) -> Dict[str, Any]:
        m = "" if method is None else str(method)
        if m in self.blocked_methods:
            return {"allowed": False, "reason": f"method {m} blocked"}
        if m in self.rate_limits:
            limit = int(self.rate_limits[m])
            recent = sum(1 for c in self.call_log[-100:] if c.get("method") == m)
            if recent >= limit:
                return {"allowed": False, "reason": f"rate limit exceeded for {m}"}
        try:
            ps = str(params)[:200]
        except Exception:
            ps = "<unprintable>"
        self.call_log.append({"method": m, "params_preview": ps})
        return {"allowed": True}


def _verdict_name(verdict: Any) -> str:
    if hasattr(verdict, "name"):
        return str(verdict.name)
    return str(verdict)


class SigmaMCPServer:
    """Creation OS MCP-shaped JSON-RPC server with σ-gate on each ``tools/call``."""

    PROTOCOL_VERSION = PROTOCOL_VERSION

    def __init__(self, gate: Any, tools: Optional[ToolsDict] = None) -> None:
        self.gate = gate
        self.tools: ToolsDict = dict(tools or {})
        self.trust_firewall = SigmaTrustFirewall()

    def handle_request(self, request: Dict[str, Any]) -> Dict[str, Any]:
        if not isinstance(request, dict):
            return self.error_response(None, -32600, "Invalid Request")
        method = request.get("method")
        params = request.get("params") or {}
        if not isinstance(params, dict):
            params = {}
        req_id = request.get("id")

        trust = self.trust_firewall.check(method if isinstance(method, str) else None, params)
        if not trust["allowed"]:
            return self.error_response(req_id, -32600, str(trust.get("reason", "rejected")))

        if method == "initialize":
            return self.handle_initialize(req_id, params)
        if method == "tools/list":
            return self.handle_tools_list(req_id)
        if method == "tools/call":
            return self.handle_tool_call(req_id, params)
        if method == "resources/list":
            return self.handle_resources_list(req_id)
        if method == "resources/read":
            return self.handle_resource_read(req_id, params)
        if method == "prompts/list":
            return self.handle_prompts_list(req_id)
        if method == "sigma/score":
            return self.handle_sigma_score(req_id, params)
        return self.error_response(req_id, -32601, f"Unknown method: {method}")

    def handle_initialize(self, req_id: Any, params: Dict[str, Any]) -> Dict[str, Any]:
        _ = params
        return self.success_response(
            req_id,
            {
                "protocolVersion": self.PROTOCOL_VERSION,
                "serverInfo": {"name": "creation-os-sigma-gate", "version": "1.0.0"},
                "capabilities": {
                    "tools": {"listChanged": True},
                    "resources": {"subscribe": True},
                    "prompts": {"listChanged": True},
                    "sigma": {"scoring": True, "cascade": True},
                },
            },
        )

    def handle_tools_list(self, req_id: Any) -> Dict[str, Any]:
        tool_list: List[Dict[str, Any]] = []
        for name, tool in self.tools.items():
            tool_list.append(
                {
                    "name": name,
                    "description": tool.get("description", ""),
                    "inputSchema": tool.get("schema", {}),
                }
            )
        return self.success_response(req_id, {"tools": tool_list})

    def handle_tool_call(self, req_id: Any, params: Dict[str, Any]) -> Dict[str, Any]:
        tool_name = str(params.get("name", ""))
        arguments = params.get("arguments") or {}
        if not isinstance(arguments, dict):
            arguments = {}
        if tool_name not in self.tools:
            return self.error_response(req_id, -32602, f"Unknown tool: {tool_name}")

        sigma, verdict = self.gate.score(f"tool:{tool_name}", json.dumps(arguments))
        vname = _verdict_name(verdict)
        if vname == "ABSTAIN":
            return self.success_response(
                req_id,
                {
                    "content": [{"type": "text", "text": "σ-gate ABSTAIN: tool call rejected"}],
                    "sigma": float(sigma),
                    "verdict": vname,
                },
            )

        handler = self.tools[tool_name].get("handler")
        if not callable(handler):
            return self.error_response(req_id, -32603, f"Tool has no handler: {tool_name}")
        result = handler(arguments)
        return self.success_response(
            req_id,
            {
                "content": [{"type": "text", "text": str(result)}],
                "sigma": float(sigma),
                "verdict": vname,
            },
        )

    def handle_resources_list(self, req_id: Any) -> Dict[str, Any]:
        return self.success_response(req_id, {"resources": []})

    def handle_resource_read(self, req_id: Any, params: Dict[str, Any]) -> Dict[str, Any]:
        _ = params
        return self.success_response(req_id, {"contents": []})

    def handle_prompts_list(self, req_id: Any) -> Dict[str, Any]:
        return self.success_response(req_id, {"prompts": []})

    def handle_sigma_score(self, req_id: Any, params: Dict[str, Any]) -> Dict[str, Any]:
        prompt = str(params.get("prompt", ""))
        response = str(params.get("response", ""))
        sigma, verdict = self.gate.score(prompt, response)
        cascade_fn = getattr(self.gate, "cascade_details", None)
        if callable(cascade_fn):
            try:
                cd = cascade_fn()
            except Exception:
                cd = {}
        else:
            cd = {}
        return self.success_response(
            req_id,
            {"sigma": float(sigma), "verdict": _verdict_name(verdict), "cascade": cd},
        )

    def register_tool(
        self,
        name: str,
        description: str,
        schema: Dict[str, Any],
        handler: Callable[[Dict[str, Any]], Any],
    ) -> None:
        self.tools[str(name)] = {"description": description, "schema": schema, "handler": handler}

    @staticmethod
    def success_response(req_id: Any, result: Any) -> Dict[str, Any]:
        return {"jsonrpc": "2.0", "id": req_id, "result": result}

    @staticmethod
    def error_response(req_id: Any, code: int, message: str) -> Dict[str, Any]:
        return {"jsonrpc": "2.0", "id": req_id, "error": {"code": code, "message": message}}


class SigmaA2AAgent:
    """Lab A2A: Agent Card JSON, optional Agent Card discovery, delegated task echo."""

    def __init__(self, agent_id: str, gate: Any, capabilities: Optional[List[str]] = None) -> None:
        self.id = str(agent_id)
        self.gate = gate
        self.capabilities = list(capabilities or [])
        self.known_agents: Dict[str, Dict[str, Any]] = {}

    def agent_card(self) -> Dict[str, Any]:
        return {
            "name": self.id,
            "description": "Creation OS agent with σ-gate (lab)",
            "url": f"https://agents.creation-os.example/{self.id}",
            "version": "1.0.0",
            "capabilities": {
                "streaming": True,
                "pushNotifications": False,
                "stateTransitionHistory": True,
            },
            "skills": [{"id": cap, "name": cap} for cap in self.capabilities],
            "sigma_gate": {
                "enabled": True,
                "cascade_levels": 5,
                "note": "Interrupt reference is python/cos/sigma_gate.h (not modified by this lab)",
            },
        }

    def fetch_agent_card(self, agent_url: str, *, timeout_s: float = 8.0) -> Optional[Dict[str, Any]]:
        base = str(agent_url).rstrip("/")
        for suffix in ("/.well-known/agent.json", "/.well-known/agent-card.json", ""):
            url = base + suffix if suffix else base
            try:
                req = urllib.request.Request(url, headers={"Accept": "application/json"})
                with urllib.request.urlopen(req, timeout=timeout_s) as resp:
                    raw = resp.read().decode("utf-8", errors="replace")
                    data = json.loads(raw)
                    if isinstance(data, dict):
                        return data
            except (urllib.error.URLError, urllib.error.HTTPError, TimeoutError, json.JSONDecodeError, ValueError):
                continue
        return None

    def discover(self, agent_url: str) -> Optional[Dict[str, Any]]:
        card = self.fetch_agent_card(agent_url)
        if card:
            key = str(card.get("name") or card.get("id") or agent_url)
            self.known_agents[key] = card
        return card

    def send_task(self, agent: Dict[str, Any], task: Any) -> Dict[str, Any]:
        _ = agent
        return {"status": "lab_echo", "task": dict(task) if isinstance(task, dict) else str(task)}

    def execute(self, task: Any) -> Dict[str, Any]:
        body = json.dumps(task) if not isinstance(task, str) else task
        return {"executed": True, "summary": body[:500]}

    def delegate(self, agent_id: str, task: Any) -> Dict[str, Any]:
        agent = self.known_agents.get(agent_id)
        if not agent:
            return {"delegated": False, "reason": "agent not found"}

        sigma, verdict = self.gate.score(f"delegate to {agent_id}", json.dumps(task))
        if _verdict_name(verdict) == "ABSTAIN":
            return {"delegated": False, "reason": "σ-gate ABSTAIN", "sigma": float(sigma)}

        result = self.send_task(agent, task)
        rs, rv = self.gate.score(json.dumps(task), json.dumps(result))
        return {
            "delegated": True,
            "result": result,
            "delegation_sigma": float(sigma),
            "result_sigma": float(rs),
            "result_verdict": _verdict_name(rv),
        }

    def receive_task(self, task: Any) -> Dict[str, Any]:
        sigma, verdict = self.gate.score("incoming_task", json.dumps(task))
        if _verdict_name(verdict) == "ABSTAIN":
            return {"accepted": False, "reason": "σ-gate rejects incoming task"}
        result = self.execute(task)
        return {"accepted": True, "result": result, "sigma": float(sigma)}


def default_sigma_tools(gate: Any) -> ToolsDict:
    """Built-in lab tools for ``cos mcp --call`` demos."""

    def _score(args: Dict[str, Any]) -> str:
        p = str(args.get("prompt", ""))
        r = str(args.get("response", ""))
        s, v = gate.score(p, r)
        return json.dumps({"sigma": float(s), "verdict": _verdict_name(v)})

    def _bench(args: Dict[str, Any]) -> str:
        _ = args
        return json.dumps(
            {
                "dataset": str(args.get("dataset", "truthfulqa")),
                "status": "not_run",
                "note": "Wire harness scripts; see docs/CLAIM_DISCIPLINE.md — no fabricated scores.",
            }
        )

    def _explain(args: Dict[str, Any]) -> str:
        p = str(args.get("prompt", ""))
        r = str(args.get("response", ""))
        return json.dumps({"mode": "lab_stub", "prompt_len": len(p), "response_len": len(r)})

    def _chat(args: Dict[str, Any]) -> str:
        p = str(args.get("prompt", ""))
        return json.dumps({"echo": p[:800], "note": "lab stub; swap for local LLM"})

    return {
        "sigma_score": {
            "description": "Score (prompt, response) with σ-gate",
            "schema": {"type": "object", "properties": {"prompt": {"type": "string"}, "response": {"type": "string"}}},
            "handler": _score,
        },
        "sigma_bench": {
            "description": "Benchmark harness placeholder — does not return dataset scores",
            "schema": {"type": "object", "properties": {"dataset": {"type": "string"}}},
            "handler": _bench,
        },
        "sigma_explain": {
            "description": "Explainability lab stub",
            "schema": {"type": "object", "properties": {"prompt": {"type": "string"}, "response": {"type": "string"}}},
            "handler": _explain,
        },
        "cos_chat": {
            "description": "Lab chat echo (σ-gated at MCP envelope)",
            "schema": {"type": "object", "properties": {"prompt": {"type": "string"}}},
            "handler": _chat,
        },
    }


__all__ = [
    "PROTOCOL_VERSION",
    "SigmaA2AAgent",
    "SigmaMCPServer",
    "SigmaTrustFirewall",
    "default_sigma_tools",
]
