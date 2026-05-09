# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.theory.active_inference import ActiveInference  # noqa: E402


class _TrendGate:
    """Deterministic σ for perception trend tests."""

    def score(self, a: str, b: str):  # noqa: ARG002
        bs = str(b)
        if "early" in bs:
            return (0.9, "ACCEPT")
        if "late" in bs:
            return (0.2, "ACCEPT")
        return (0.5, "ACCEPT")


class _WM:
    def predict(self, beliefs: str, action: str):  # noqa: ARG002
        if str(action) == "low":
            return {"predicted": "target_a", "σ": 0.1}
        return {"predicted": "target_b", "σ": 0.9}


def test_perceive_returns_sigma() -> None:
    ai = ActiveInference()
    r = ai.perceive("hello")
    assert "σ" in r and "sigma" in r
    assert isinstance(r["σ"], float)
    assert r["verdict"]


def test_set_preference() -> None:
    ai = ActiveInference()
    ai.set_preference("goal", "stable")
    assert ai.preferences["goal"] == "stable"


def test_evaluate_actions_ranks() -> None:
    ai = ActiveInference(world_model=_WM())
    scored = ai.evaluate_actions(["high", "low"])
    assert scored[0]["action"] == "low"
    assert scored[0]["expected_σ"] <= scored[-1]["expected_σ"]


def test_act_selects_lowest_sigma() -> None:
    ai = ActiveInference(world_model=_WM())
    out = ai.act(["high", "low"])
    assert out["chosen_action"] == "low"


def test_update_closes_loop() -> None:
    ai = ActiveInference()
    ai.perceive("in")
    ai.act(["only"])
    u = ai.update("feedback")
    assert u["belief_updated"] is True
    assert "prediction_error" in u


def test_loop_runs_n_steps() -> None:
    ai = ActiveInference(gate=_TrendGate())

    def actions(_obs: str):
        return ["a", "b"]

    obs = ["early", "late", "late"]
    out = ai.loop(obs, actions, n_steps=2)
    assert len(out["steps"]) == 2


def test_free_energy_calculation() -> None:
    ai = ActiveInference()
    ai.perceive("x")
    ai.perceive("y")
    fe = ai.free_energy()
    assert isinstance(fe, float)
    assert 0.0 <= fe <= 1.0


def test_sigma_trend_improving() -> None:
    ai = ActiveInference(gate=_TrendGate())

    def actions(_obs: str):
        return ["a"]

    out = ai.loop(["early", "late"], actions, n_steps=2)
    assert out["σ_trend"] == "improving"
    assert out["sigma_trend"] == "improving"
