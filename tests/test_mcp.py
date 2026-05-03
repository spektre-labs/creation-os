# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import json
import tempfile
from pathlib import Path

from cos.mcp import PROTOCOL_VERSION, SigmaA2AAgent, SigmaMCPServer


def test_protocol_version_constant() -> None:
    assert PROTOCOL_VERSION == "2025-11-25"


def test_capability_negotiation_shapes() -> None:
    s = SigmaMCPServer()
    cap = s.capability_negotiation({})
    assert cap["protocolVersion"] == PROTOCOL_VERSION
    assert cap["ready"] is True
    assert "tools" in cap["capabilities"]


def test_primitives_manifest() -> None:
    s = SigmaMCPServer()
    m = s.primitives_manifest()
    assert "tools" in m["primitives"]


def test_score_tool_call_includes_verdict() -> None:
    s = SigmaMCPServer()
    r = s.score_tool_call("read_docs", {"q": "x"})
    assert "sigma" in r and "verdict" in r
    assert "annotations" in r


def test_score_resource_read() -> None:
    s = SigmaMCPServer()
    r = s.score_resource_read("file:///etc/passwd")
    assert r["resource_uri"].startswith("file://")


def test_score_sampling() -> None:
    s = SigmaMCPServer()
    r = s.score_sampling("2+2?", "4")
    assert 0.0 <= r["sigma"] <= 1.0


def test_trust_firewall_blocks_method() -> None:
    s = SigmaMCPServer()
    s.block_method("evil/call")
    assert s.trust_firewall_check("evil/call")["allow"] is False


def test_trust_firewall_rate_limit() -> None:
    s = SigmaMCPServer()
    s.rpm_limit_per_key = 2
    assert s.trust_firewall_check("x")["allow"] is True
    assert s.trust_firewall_check("x")["allow"] is True
    assert s.trust_firewall_check("x")["allow"] is False


def test_tool_annotations_destructive() -> None:
    s = SigmaMCPServer()
    a = s.tool_annotations("noop", destructive_hint=True)
    assert a["reversibility"] == "irreversible"


def test_a2a_discover_file_and_delegate() -> None:
    card = {"name": "lab-agent", "skills": []}
    with tempfile.TemporaryDirectory() as td:
        p = Path(td) / "agent.json"
        p.write_text(json.dumps(card), encoding="utf-8")
        url = p.as_uri()
        a2a = SigmaA2AAgent()
        d = a2a.discover(url)
        assert d.get("name") == "lab-agent"
        dg = a2a.delegate("peer-1", "summarize doc")
        assert "sigma" in dg and "allowed" in dg
        rv = a2a.receive_task("incoming task")
        assert "accept" in rv
