# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Lab tests for JSON-RPC σ-MCP v2 handler and continual v2 orchestrator."""
from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.sigma_continual import SigmaContinualLearning, ToyContinualGate, ToyContinualModel  # noqa: E402
from cos.sigma_mcp import (  # noqa: E402
    PROTOCOL_VERSION,
    SigmaMCPServer,
    SigmaTrustFirewall,
    default_sigma_tools,
)
from cos.sigma_twin import TwinGateLab  # noqa: E402


def test_sigma_mcp_jsonrpc_tools_call() -> None:
    gate = TwinGateLab()
    srv = SigmaMCPServer(gate, default_sigma_tools(gate))
    req = {
        "jsonrpc": "2.0",
        "id": 1,
        "method": "tools/call",
        "params": {"name": "sigma_score", "arguments": {"prompt": "hello", "response": "hi"}},
    }
    out = srv.handle_request(req)
    assert out.get("result") and "sigma" in str(out["result"]).lower()


def test_sigma_mcp_trust_firewall_blocks() -> None:
    gate = TwinGateLab()
    srv = SigmaMCPServer(gate, default_sigma_tools(gate))
    srv.trust_firewall.block_method("tools/call")
    req = {
        "jsonrpc": "2.0",
        "id": 2,
        "method": "tools/call",
        "params": {"name": "sigma_score", "arguments": {}},
    }
    out = srv.handle_request(req)
    assert "error" in out


def test_sigma_mcp_initialize_protocol_tag() -> None:
    gate = TwinGateLab()
    srv = SigmaMCPServer(gate, {})
    out = srv.handle_request({"jsonrpc": "2.0", "id": 0, "method": "initialize", "params": {}})
    pv = out["result"]["protocolVersion"]
    assert pv == PROTOCOL_VERSION


def test_sigma_trust_firewall_rate_limit() -> None:
    fw = SigmaTrustFirewall()
    fw.set_rate_limit("sigma/score", 2)
    assert fw.check("sigma/score", {})["allowed"]
    assert fw.check("sigma/score", {})["allowed"]
    assert not fw.check("sigma/score", {})["allowed"]


def test_sigma_continual_v2_learn_task() -> None:
    model = ToyContinualModel()
    gate = ToyContinualGate(model)
    orch = SigmaContinualLearning(gate, model, lambda_ewc=10.0)
    out = orch.learn_task(
        [{"prompt": "unit A", "response": "ok"}, {"prompt": "unit B", "response": "yes"}],
        "task_test",
    )
    assert out.get("task") == "task_test"
    assert "regression_detected" in out
