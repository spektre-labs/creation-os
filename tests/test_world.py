# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from cos.graph import SigmaGraph
from cos.world import SigmaWorld, WorldState


def test_observe() -> None:
    world = SigmaWorld()
    state = world.observe("User asked about quantum computing")
    assert isinstance(state, WorldState)
    assert len(world.states) == 1


def test_transition() -> None:
    world = SigmaWorld()
    world.observe("Initial state")
    result = world.transition("Ask a follow-up question")
    assert "sigma" in result
    assert "verdict" in result


def test_plan() -> None:
    world = SigmaWorld()
    world.observe("Starting point")
    plan = world.plan("Reach the goal")
    assert "steps" in plan
    assert "feasible" in plan
    assert len(plan["steps"]) >= 1


def test_feature_trend() -> None:
    world = SigmaWorld()
    world.observe("Step 1", features={"confidence": 0.5})
    world.observe("Step 2", features={"confidence": 0.7})
    world.observe("Step 3", features={"confidence": 0.9})
    pred = world.forecast("next?")
    assert "feature_trend" in pred


def test_commonsense_plausible() -> None:
    w = SigmaWorld()
    r = w.commonsense_check("Water flows downhill under gravity", "")
    assert "plausible" in r and "σ" in r


def test_commonsense_contradiction() -> None:
    g = SigmaGraph(write_threshold=1.0)
    g.add("ice", "melts_at", "warm", sigma=0.2)
    w = SigmaWorld(graph=g)
    r = w.commonsense_check("ice melts_at freezing in all conditions")
    assert r.get("plausible") is False or r.get("verdict") == "RETHINK"


def test_predict_returns_sigma() -> None:
    w = SigmaWorld()
    r = w.predict("room is dark", "flip light switch")
    assert "σ" in r and "verdict" in r and "predicted_outcome" in r


def test_simulate_accumulates_sigma() -> None:
    w = SigmaWorld()
    out = w.simulate("start", ["a", "b"])
    assert out["final_σ"] >= 0
    assert out["steps"] == 2


def test_simulate_trajectory() -> None:
    w = SigmaWorld()
    out = w.simulate("s0", ["m1"])
    assert len(out["trajectory"]) == 1
    assert out["trajectory"][0]["action"] == "m1"


def test_no_graph_falls_back() -> None:
    w = SigmaWorld(graph=None)
    r = w.commonsense_check("birds typically have feathers")
    assert "σ" in r
    assert r.get("verdict") is not None
