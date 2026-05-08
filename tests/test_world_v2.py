# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import pytest

from cos.sigma_gate import SigmaGate
from cos.world import SigmaWorldV2


def test_observe_updates_state() -> None:
    w = SigmaWorldV2()
    r = w.observe("door is open")
    assert r["state_updated"] is True
    assert "σ" in r
    assert w.state.get("last") == "door is open"


def test_learn_dynamics() -> None:
    w = SigmaWorldV2()
    out = w.learn_dynamics("push ball", "ball rolls")
    assert out["learned"] is True
    assert len(w.dynamics) == 1
    assert "σ" in w.dynamics[0]


def test_simulate_trajectory() -> None:
    w = SigmaWorldV2()
    w.observe("robot at origin")
    w.learn_dynamics("move north", "y increases")
    out = w.simulate("move north", n_steps=3)
    assert out["horizon"] >= 1
    assert len(out["trajectory"]) >= 1
    assert "avg_σ" in out


def test_simulate_stops_at_abstain(monkeypatch: pytest.MonkeyPatch) -> None:
    gate = SigmaGate()
    monkeypatch.setattr(gate, "score", lambda _p, _r: (0.95, "ABSTAIN"))
    w = SigmaWorldV2(gate=gate)
    w.observe("x")
    out = w.simulate("act", n_steps=5)
    assert any(t.get("HORIZON_LIMIT") for t in out["trajectory"])
    assert out["horizon"] >= 1


def test_plan_selects_lowest_sigma(monkeypatch: pytest.MonkeyPatch) -> None:
    gate = SigmaGate()

    def score(_p: str, r: str) -> tuple[float, str]:
        rlow = str(r).lower()
        if "cheap" in rlow:
            return 0.1, "ACCEPT"
        return 0.88, "ABSTAIN"

    monkeypatch.setattr(gate, "score", score)
    w = SigmaWorldV2(gate=gate)
    w.state["last"] = "here"
    out = w.plan(["heavy", "cheap_move"], n_steps=2)
    assert out["best_action"] == "cheap_move"
    assert out["best_σ"] < 0.5


def test_counterfactual_regret(monkeypatch: pytest.MonkeyPatch) -> None:
    gate = SigmaGate()

    def score(_p: str, r: str) -> tuple[float, str]:
        if "taken" in str(r) or "taken_route" in str(r).lower():
            return 0.7, "RETHINK"
        return 0.2, "ACCEPT"

    monkeypatch.setattr(gate, "score", score)
    w = SigmaWorldV2(gate=gate)
    w.state["last"] = "s0"
    out = w.counterfactual("taken_route", "alt_route")
    assert out["better_alternative"] is True
    assert out["regret"] >= 0


def test_dream_consolidates() -> None:
    w = SigmaWorldV2()
    w.dynamics.append({"cause": "a", "effect": "b", "σ": 0.9, "reliable": False})
    w.simulations.append({"action": "x", "trajectory": [], "avg_σ": 0.1})
    w.simulations.append({"action": "y", "trajectory": [], "avg_σ": 0.95})
    r = w.dream()
    assert r["dreamed"] is True
    assert r["dynamics_pruned"] >= 1
    assert r["consolidated"] >= 1


def test_imagination_budget() -> None:
    w = SigmaWorldV2()
    w.learn_dynamics("c1", "e1")
    b = w.imagination_budget()
    assert b["dynamics_total"] == 1
    assert "imagination_power" in b
