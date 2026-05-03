# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
σ-gated **MCP marketplace** lab surface: registry helpers, tool I/O envelopes, and a tiny
HTTP JSON-RPC **ping** stub for integration tests.

Full MCP stdio servers remain in ``mcp_sigma_server`` / ``creation_os_sigma_mcp``.
See ``docs/CLAIM_DISCIPLINE.md`` — throughput / adoption claims are not asserted here.
"""
from __future__ import annotations

import hashlib
import json
from http.server import BaseHTTPRequestHandler, HTTPServer
from typing import Any, Dict, Tuple
from urllib.error import URLError
from urllib.request import Request, urlopen

from cos.sigma_a2a_card import build_sigma_verifier_agent_card


def sigma_gated_tool_payload(sigma: float, tau: float, payload: Dict[str, Any]) -> Tuple[str, Dict[str, Any]]:
    """
    If ``sigma < tau`` (low trust), emit ``RETHINK`` wrapper; else ``ACCEPT``.
    ``sigma`` is a trust score in ``[0,1]`` where **higher is safer** in this lab convention.
    """
    s, t = float(sigma), float(tau)
    if s < t:
        return "RETHINK", {"reason": "sigma_below_tau", "sigma": s, "tau": t, "data": payload}
    return "ACCEPT", {"sigma": s, "data": payload}


def tool_sigma_check_lab(prompt: str, response: str) -> Dict[str, Any]:
    """Deterministic toy σ for MCP ``sigma_check`` tool body."""
    h = hashlib.sha256(f"{prompt}|{response}".encode()).digest()
    sigma = int.from_bytes(h[:2], "big") / 65535.0
    return {"sigma": round(float(sigma), 6), "prompt_sha256": hashlib.sha256(prompt.encode()).hexdigest()[:16]}


def tool_engram_query_lab(query: str, threshold: float = 0.2) -> Dict[str, Any]:
    strength = float(int.from_bytes(hashlib.sha256(query.encode()).digest()[:2], "big") / 65535.0)
    return {"hit": strength >= float(threshold), "strength": round(strength, 6)}


def tool_jepa_plan_stub(goal: str, horizon: int = 3) -> Dict[str, Any]:
    return {"goal": goal, "horizon": int(horizon), "note": "wire SigmaJEPA.plan_argmin_sigma in harness"}


def build_creation_os_mcp_catalog() -> Dict[str, Any]:
    return {
        "name": "creation-os-mcp-lab",
        "tools": [
            {"name": "sigma_check", "description": "Hash-shaped σ for (prompt, response) pair (lab)."},
            {"name": "engram_query", "description": "Toy recall strength vs threshold (lab)."},
            {"name": "jepa_plan", "description": "Planning stub; replace with SigmaJEPA in product."},
        ],
        "transports": ["stdio", "sse", "http-jsonrpc-lab"],
    }


def mcp_connect_probe(url: str, *, timeout_s: float = 5.0) -> Dict[str, Any]:
    """GET ``url`` (or append ``/health``) — connectivity smoke for ``cos mcp connect``."""
    u = url.strip().rstrip("/")
    last = ""
    for candidate in (u, f"{u}/health"):
        try:
            req = Request(candidate, headers={"User-Agent": "creation-os-cos-mcp"})
            with urlopen(req, timeout=timeout_s) as resp:
                body = resp.read(4096).decode("utf-8", errors="replace")
                status = int(getattr(resp, "status", 200))
            return {"ok": True, "url": candidate, "status": status, "snippet": body[:200]}
        except URLError as e:
            last = str(e)
        except OSError as e:
            last = str(e)
    return {"ok": False, "url": url, "error": last}


def serve_mcp_lab_http(*, host: str = "127.0.0.1", port: int = 8080) -> None:
    """
    Minimal JSON-RPC 2.0 loopback: ``ping`` method. Not a full MCP SDK server.
    """

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, fmt: str, *args: Any) -> None:
            return

        def do_GET(self) -> None:  # noqa: N802
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(b'{"service":"creation-os-mcp-lab","ok":true}\n')

        def do_POST(self) -> None:  # noqa: N802
            n = int(self.headers.get("Content-Length", "0") or 0)
            raw = self.rfile.read(n).decode("utf-8", errors="replace")
            try:
                msg = json.loads(raw)
            except json.JSONDecodeError:
                msg = {}
            mid = msg.get("id")
            method = str(msg.get("method", ""))
            if method == "ping":
                body = json.dumps({"jsonrpc": "2.0", "id": mid, "result": {"pong": True}})
            elif method == "tools/list":
                body = json.dumps({"jsonrpc": "2.0", "id": mid, "result": build_creation_os_mcp_catalog()})
            else:
                body = json.dumps({"jsonrpc": "2.0", "id": mid, "error": {"code": -32601, "message": "method not found"}})
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(body.encode("utf-8"))

    httpd = HTTPServer((host, int(port)), Handler)
    httpd.serve_forever()


def default_agent_card(endpoint: str = "https://localhost/creation-os") -> Dict[str, Any]:
    """σ verifier card using shared helper (no embedded benchmark numbers)."""
    return build_sigma_verifier_agent_card(
        agent_id="creation_os_verifier",
        name="Creation OS σ Verifier",
        description="σ-gate interrupt + lab tools (MCP / A2A scaffolding).",
        endpoint=endpoint,
        capabilities=["hallucination_detection", "steering", "memory", "planning"],
        signals=["lsd", "hide", "entropy", "sae"],
    )


__all__ = [
    "build_creation_os_mcp_catalog",
    "default_agent_card",
    "mcp_connect_probe",
    "serve_mcp_lab_http",
    "sigma_gated_tool_payload",
    "tool_engram_query_lab",
    "tool_jepa_plan_stub",
    "tool_sigma_check_lab",
]
