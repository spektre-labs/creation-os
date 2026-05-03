# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from cos.world import SigmaWorld, WorldState


def test_observe() -> None:
    world = SigmaWorld()
    state = world.observe("User asked about quantum computing")
    assert isinstance(state, WorldState)
    assert len(world.states) == 1


def test_simulate() -> None:
    world = SigmaWorld()
    world.observe("Initial state")
    result = world.simulate("Ask a follow-up question")
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
    pred = world.predict("next?")
    assert "feature_trend" in pred
