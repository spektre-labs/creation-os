# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import pytest

from cos.causal import PearlDAG, PearlLadder


def test_see_returns_association() -> None:
    pl = PearlLadder()
    r = pl.see("smoking", "cancer")
    assert r["rung"] == 1
    assert r["type"] == "association"
    assert "σ" in r and "verdict" in r


def test_do_returns_intervention() -> None:
    pl = PearlLadder()
    r = pl.do("treatment", "recovery")
    assert r["rung"] == 2
    assert "σ_do" in r and "σ_see" in r


def test_imagine_returns_counterfactual() -> None:
    pl = PearlLadder()
    r = pl.imagine("sunny", "dry", "rainy")
    assert r["rung"] == 3
    assert "σ_imagine" in r and "σ_do" in r and "σ_see" in r


def test_sigma_increases_up_ladder(monkeypatch: pytest.MonkeyPatch) -> None:
    gate = __import__("cos.sigma_gate", fromlist=["SigmaGate"]).SigmaGate()
    idx = {"i": 0}

    def rising(_p: str, _r: str) -> tuple[float, str]:
        idx["i"] += 1
        return min(0.05 + idx["i"] * 0.08, 0.99), "ACCEPT"

    monkeypatch.setattr(gate, "score", rising)
    pl = PearlLadder(gate=gate)
    s = pl.ladder_summary("X", "Y")
    s1 = float(s["L1_see"]["σ_see"])
    d2 = float(s["L2_do"]["σ_do"])
    i3 = float(s["L3_imagine"]["σ_imagine"])
    assert s1 < d2 <= i3


def test_confounder_detected() -> None:
    g = PearlDAG()
    g.add_confounder("ice", "slip", "temperature")
    pl = PearlLadder(graph=g)
    r = pl.see("ice", "slip")
    assert r["confounded"] is True
    assert r["warning"] is not None


def test_do_calculus_adjusts_for_confounder(monkeypatch: pytest.MonkeyPatch) -> None:
    from cos.sigma_gate import SigmaGate

    gate = SigmaGate()

    def fixed(_p: str, _r: str) -> tuple[float, str]:
        return 0.5, "RETHINK"

    monkeypatch.setattr(gate, "score", fixed)
    plain = PearlLadder(gate=gate)
    d0 = plain.do("a", "b")["σ_do"]

    g = PearlDAG()
    g.add_confounder("a", "b", "u")
    pl2 = PearlLadder(gate=gate, graph=g)
    r2 = pl2.do("a", "b")
    d1 = r2["σ_do"]
    assert d1 >= d0
    assert r2["do_calculus_applied"] is True


def test_root_cause_traces_back() -> None:
    g = PearlDAG()
    g.add("clouds", "rain", σ=0.2)
    g.add("rain", "wet", σ=0.15)
    pl = PearlLadder(graph=g)
    out = pl.root_cause("wet", max_depth=5)
    assert out["depth"] >= 1
    assert len(out["path"]) >= 1
    assert out["root_cause"] is not None


def test_ladder_summary_all_rungs() -> None:
    pl = PearlLadder()
    s = pl.ladder_summary("policy", "gdp")
    assert "L1_see" in s and "L2_do" in s and "L3_imagine" in s
