# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import pytest

from cos.cascade_router import SigmaCascade


def test_route_resolves_at_fast(monkeypatch: pytest.MonkeyPatch) -> None:
    from cos.sigma_gate import SigmaGate

    gate = SigmaGate()

    def always_ok(_p: str, _r: str) -> tuple[float, str]:
        return 0.05, "ACCEPT"

    monkeypatch.setattr(gate, "score", always_ok)
    c = SigmaCascade(gate=gate)
    c.add_level("FAST", model_fn=lambda p: f"ok:{p}", cost=0.001, max_σ=0.2)
    out = c.route("hello")
    assert out["resolved_at"] == "FAST"
    assert out["levels_tried"] == 1
    assert "ok:hello" in str(out["response"])


def test_route_escalates_on_high_sigma(monkeypatch: pytest.MonkeyPatch) -> None:
    from cos.sigma_gate import SigmaGate

    gate = SigmaGate()
    calls: list[tuple[float, str]] = [(0.9, "ABSTAIN"), (0.1, "ACCEPT")]
    i = {"n": 0}

    def seq_score(_p: str, _r: str) -> tuple[float, str]:
        j = i["n"]
        i["n"] += 1
        return calls[min(j, len(calls) - 1)]

    monkeypatch.setattr(gate, "score", seq_score)
    c = SigmaCascade(gate=gate)
    c.add_level("A", model_fn=lambda p: f"a({p})", cost=0.001, max_σ=0.15)
    c.add_level("B", model_fn=lambda p: f"b({p})", cost=0.01, max_σ=0.5)
    out = c.route("q")
    assert out["resolved_at"] == "B"
    assert out["levels_tried"] == 2
    assert str(out["response"]).startswith("b(")


def test_route_abstains_when_all_fail(monkeypatch: pytest.MonkeyPatch) -> None:
    from cos.sigma_gate import SigmaGate

    gate = SigmaGate()
    monkeypatch.setattr(gate, "score", lambda _p, _r: (0.99, "ABSTAIN"))
    c = SigmaCascade(gate=gate)
    c.add_level("X", model_fn=lambda p: p, cost=0.001, max_σ=0.01)
    c.add_level("Y", model_fn=lambda p: p, cost=0.002, max_σ=0.01)
    out = c.route("z")
    assert out["verdict"] == "ABSTAIN"
    assert out["response"] is None
    assert out["levels_tried"] == 2


def test_stats_cost_savings(monkeypatch: pytest.MonkeyPatch) -> None:
    from cos.sigma_gate import SigmaGate

    gate = SigmaGate()
    monkeypatch.setattr(gate, "score", lambda _p, _r: (0.05, "ACCEPT"))
    c = SigmaCascade(gate=gate)
    c.add_level("cheap", model_fn=lambda p: p, cost=0.001, max_σ=0.5)
    c.add_level("dear", model_fn=lambda p: p, cost=0.01, max_σ=0.5)
    c.route("a")
    c.route("b")
    st = c.stats()
    assert st["total_queries"] == 2
    assert st["baseline_cost"] == pytest.approx(0.02, rel=1e-6)
    assert st["savings"] >= 0


def test_default_cascade_four_routing_tiers() -> None:
    c = SigmaCascade.default_cascade()
    assert len(c.levels) == 4
    names = [lv.name for lv in c.levels]
    assert names == ["FAST", "VERIFY", "RAG", "ESCALATE"]


def test_trace_records_all_levels(monkeypatch: pytest.MonkeyPatch) -> None:
    from cos.sigma_gate import SigmaGate

    gate = SigmaGate()
    monkeypatch.setattr(gate, "score", lambda _p, _r: (0.99, "ABSTAIN"))
    c = SigmaCascade(gate=gate)
    c.add_level("L1", lambda p: p, 0.001, 0.01)
    c.add_level("L2", lambda p: p, 0.002, 0.01)
    out = c.route("x")
    assert len(out["trace"]) == 2
    assert [t["level"] for t in out["trace"]] == ["L1", "L2"]


def test_savings_percentage_near_baseline_when_always_cheapest(monkeypatch: pytest.MonkeyPatch) -> None:
    from cos.sigma_gate import SigmaGate

    gate = SigmaGate()
    monkeypatch.setattr(gate, "score", lambda _p, _r: (0.01, "ACCEPT"))
    c = SigmaCascade(gate=gate)
    c.add_level("cheap", model_fn=lambda p: p, cost=0.001, max_σ=0.5)
    c.add_level("dear", model_fn=lambda p: p, cost=0.01, max_σ=0.5)
    for _ in range(10):
        c.route("p")
    st = c.stats()
    assert st["savings_pct"] == pytest.approx(90.0, abs=0.1)
