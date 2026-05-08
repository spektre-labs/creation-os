# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from pathlib import Path

from cos.fleet import BLOCKED, SigmaFleet
from cos.sigma_gate import ABSTAIN, ACCEPT, SigmaGate


class _LowSigmaGate:
    def score(self, prompt: str, response: str):  # noqa: ARG002
        return (0.05, ACCEPT)


class _HighSigmaGate:
    def score(self, prompt: str, response: str):  # noqa: ARG002
        return (0.95, ABSTAIN)


def test_register_and_route() -> None:
    f = SigmaFleet()
    f.register("small", avg_sigma=0.2, cost=0.1)
    f.register("large", avg_sigma=0.2, cost=1.0)
    g = SigmaGate()
    r = f.route("hello", g)
    assert r["name"] in f.models


def test_cascade_prefers_cheap() -> None:
    f = SigmaFleet()
    f.register("cheap", cost=0.05)
    f.register("dear", cost=2.0)
    g = SigmaGate()
    c = f.cascade_route("2+2?", g)
    assert c["name"] is not None


def test_cascade_empty() -> None:
    f = SigmaFleet()
    g = SigmaGate()
    c = f.cascade_route("x", g)
    assert c["name"] is None


def test_cost_tracking() -> None:
    f = SigmaFleet()
    f.cost_tracking("m", 1.5)
    assert len(f._cost_log) == 1


def test_sigma_portfolio() -> None:
    f = SigmaFleet()
    f.register("a", avg_sigma=0.3)
    p = f.sigma_portfolio()
    assert p["a"] == 0.3


def test_auto_fallback() -> None:
    f = SigmaFleet()
    f.register("first", cost=1.0)
    f.register("second", cost=2.0)
    g = SigmaGate()
    r = f.auto_fallback("question", g, ["missing", "first", "second"])
    assert r["name"] in ("first", "second", None)


def test_cascade_stop_flag() -> None:
    f = SigmaFleet()
    f.register("only", cost=0.1)
    g = SigmaGate()
    c = f.cascade_route("Paris France capital", g)
    assert "cascade_stop" in c


def test_register_agent(tmp_path: Path) -> None:
    fl = SigmaFleet(gate=_LowSigmaGate(), audit_dir=str(tmp_path))
    a = fl.register("agent-1", "mock-model", "owner-a", tier="standard")
    assert a.agent_id == "agent-1"
    assert "agent-1" in fl.agents


def test_score_agent_returns_sigma(tmp_path: Path) -> None:
    fl = SigmaFleet(gate=_LowSigmaGate(), audit_dir=str(tmp_path))
    fl.register("a2", "m", "o")
    r = fl.score_agent("a2", "p", "r")
    assert r["σ"] < 0.2
    assert r["verdict"] == ACCEPT


def test_fleet_status_dashboard(tmp_path: Path) -> None:
    fl = SigmaFleet(gate=_LowSigmaGate(), audit_dir=str(tmp_path))
    fl.register("x", "mx", "ox")
    fl.score_agent("x", "hi", "there")
    s = fl.fleet_status()
    assert s["total_agents"] == 1
    assert s["agents"][0]["calls"] == 1


def test_quarantine_blocks_agent(tmp_path: Path) -> None:
    fl = SigmaFleet(gate=_LowSigmaGate(), audit_dir=str(tmp_path))
    fl.register("q1", "m", "o")
    fl.quarantine("q1", reason="probe")
    r = fl.score_agent("q1", "a", "b")
    assert r["verdict"] == BLOCKED


def test_promote_requires_low_sigma(tmp_path: Path) -> None:
    bad = SigmaFleet(gate=_HighSigmaGate(), audit_dir=str(tmp_path / "b"))
    bad.register("slow", "m", "o")
    bad.score_agent("slow", "x", "y")
    assert bad.promote("slow")["promoted"] is False

    good = SigmaFleet(gate=_LowSigmaGate(), audit_dir=str(tmp_path / "g"))
    good.register("ok", "m", "o")
    good.score_agent("ok", "x", "y")
    assert good.promote("ok")["promoted"] is True


def test_sovereign_deploy(tmp_path: Path) -> None:
    fl = SigmaFleet(gate=_LowSigmaGate(), audit_dir=str(tmp_path))
    fl.register("s1", "m", "o")
    r = fl.sovereign_deploy("s1", jurisdiction="EU")
    assert r["sovereign"] is True
    assert fl.agents["s1"].tier == "sovereign"


def test_compliance_report(tmp_path: Path) -> None:
    fl = SigmaFleet(gate=_LowSigmaGate(), audit_dir=str(tmp_path))
    fl.register("c1", "m", "o")
    rep = fl.compliance_report()
    assert rep["fleet_size"] == 1
    assert "avg_σ" in rep


def test_audit_log_persists(tmp_path: Path) -> None:
    fl = SigmaFleet(gate=_LowSigmaGate(), audit_dir=str(tmp_path))
    fl.register("aud", "m", "o")
    audit_file = tmp_path / "audit.jsonl"
    assert audit_file.is_file()
    lines = audit_file.read_text(encoding="utf-8").strip().splitlines()
    assert len(lines) >= 1
    assert fl.audit_log[-1]["action"] == "register"
