# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.a2a` (v264 facade)."""
from __future__ import annotations

import json
from pathlib import Path

from cos.a2a import A2ATaskLifecycle, SigmaA2A
from cos.sigma_gate import SigmaGate


def test_agent_card_has_sigma_profile() -> None:
    a = SigmaA2A(SigmaGate(), agent_id="t1")
    c = a.agent_card()
    assert "sigma_profile" in c and "a2a_v2" in c
    assert c["trust_level"]


def test_discover_file_url(tmp_path: Path) -> None:
    card = {"agent_id": "f", "capabilities": ["hallucination_detection"]}
    p = tmp_path / "agent.json"
    p.write_text(json.dumps(card), encoding="utf-8")
    a = SigmaA2A(SigmaGate())
    r = a.discover(p.as_uri())
    assert r["ok"] is True
    assert r["card"]["agent_id"] == "f"


def test_discover_invalid_url() -> None:
    a = SigmaA2A(SigmaGate())
    r = a.discover("http://127.0.0.1:9/nope")
    assert r["ok"] is False


def test_negotiate_missing_capabilities() -> None:
    a = SigmaA2A(SigmaGate())
    agent = {"agent_id": "x", "capabilities": ["a"]}
    task = {"required_capabilities": ["a", "missing"]}
    n = a.negotiate(agent, task, {})
    assert n["can_accept"] is False
    assert "missing" in n["missing_capabilities"]


def test_delegate_completes_when_negotiate_ok() -> None:
    a = SigmaA2A(SigmaGate())
    agent = {"agent_id": "peer1", "capabilities": ["code"]}
    task = {"required_capabilities": ["code"], "goal": "lint"}
    d = a.delegate(agent, task)
    assert d["state"] == A2ATaskLifecycle.COMPLETED.value
    assert "sigma" in d


def test_delegate_fails_when_negotiate_rejects() -> None:
    a = SigmaA2A(SigmaGate())
    agent = {"agent_id": "z", "capabilities": []}
    task = {"required_capabilities": ["need"]}
    d = a.delegate(agent, task)
    assert d["state"] == A2ATaskLifecycle.FAILED.value


def test_governance_trail_records_delegate() -> None:
    a = SigmaA2A(SigmaGate())
    agent = {"agent_id": "p", "capabilities": ["c"]}
    a.delegate(agent, {"required_capabilities": ["c"], "x": 1})
    trail = a.governance_trail()
    assert any(x.get("kind") == "delegate" for x in trail)


def test_protocol_extension_shape() -> None:
    a = SigmaA2A(SigmaGate())
    ext = a.protocol_extension()
    assert "mcp_tools_endpoint" in ext and "a2a_peers" in ext


# --- σ-validated multi-agent hub (SigmaA2ANetwork) --------------------------------

from cos.a2a import AgentCard, SigmaA2ANetwork  # noqa: E402


class _ConstGate:
    def __init__(self, sigma: float, verdict: str) -> None:
        self._sigma = sigma
        self._v = verdict

    def score(self, _p: str, _r: str) -> tuple[float, str]:
        return self._sigma, self._v


def test_agent_card_to_json() -> None:
    c = AgentCard("alpha", ["code", "review"], endpoint="http://a", avg_σ=0.35)
    d = json.loads(c.to_json())
    assert d["name"] == "alpha"
    assert d["σ_profile"]["avg_σ"] == 0.35
    assert "jsonrpc" in c.jsonrpc_result()


def test_register_agent() -> None:
    me = AgentCard("me", ["coordination"], avg_σ=0.5)
    hub = SigmaA2ANetwork(me, gate=_ConstGate(0.2, "ACCEPT"))
    peer = AgentCard("peer", ["code"], avg_σ=0.4)
    hub.register_agent(peer)
    assert "peer" in hub.known_agents


def test_best_agent_for_task() -> None:
    me = AgentCard("me", ["coordination"], avg_σ=0.5)
    hub = SigmaA2ANetwork(me, gate=_ConstGate(0.1, "ACCEPT"))
    hub.register_agent(AgentCard("low", ["python"], avg_σ=0.2))
    hub.register_agent(AgentCard("high", ["python"], avg_σ=0.8))
    pick = hub.best_agent_for("need python help")
    assert pick is not None and pick.name == "low"


def test_delegate_creates_task() -> None:
    me = AgentCard("me", ["coordination"], avg_σ=0.5)
    hub = SigmaA2ANetwork(me, gate=_ConstGate(0.15, "ACCEPT"))
    hub.register_agent(AgentCard("worker", ["summarize"], avg_σ=0.3))
    out = hub.delegate("summarize this doc", execute_fn=lambda _: "ok")
    assert out["task_id"].startswith("task_")
    assert out["assignee"] == "worker"
    assert out["status"] in ("completed", "completed_uncertain")


def test_receive_result_sigma_validates() -> None:
    me = AgentCard("me", ["coordination"], avg_σ=0.5)
    hub = SigmaA2ANetwork(me, gate=_ConstGate(0.1, "ACCEPT"))
    hub.register_agent(AgentCard("w", ["fix"], avg_σ=0.3))
    hub.delegate("fix bug in fix handler")
    tid = next(iter(hub.tasks.keys()))
    r = hub.receive_result(tid, "patch applied")
    assert r.get("trusted") is True
    assert "σ" in r


def test_no_capable_agent_returns_error() -> None:
    me = AgentCard("me", ["coordination"], avg_σ=0.5)
    hub = SigmaA2ANetwork(me, gate=_ConstGate(0.5, "RETHINK"))
    out = hub.delegate("cryptic task xyz")
    assert out.get("error") == "No capable agent found"


def test_network_sigma() -> None:
    me = AgentCard("me", ["coordination"], avg_σ=0.5)
    hub = SigmaA2ANetwork(me, gate=_ConstGate(0.2, "ACCEPT"))
    hub.register_agent(AgentCard("a", ["x"], avg_σ=0.2))
    hub.register_agent(AgentCard("b", ["y"], avg_σ=0.6))
    assert hub.network_σ() == 0.4
