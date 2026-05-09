# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.embodied` (sensor fusion + toy closed-loop lab)."""
from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.integrations.embodied import EmbodiedController, SensorFusion  # noqa: E402


class _FixedSigma:
    def __init__(self, sigma: float, verdict: str = "ACCEPT") -> None:
        self._σ = float(sigma)
        self._v = verdict

    def score(self, _p: str, _r: str):
        return self._σ, self._v


class _PairConflictGate:
    """Low σ when both cam and lidar present; high σ when readings look inconsistent."""

    def score(self, prompt: str, response: str) -> tuple[float, str]:
        blob = f"{prompt}\n{response}"
        if "stale" in blob.lower() and "cam" in blob and "lidar" in blob:
            return 0.72, "RETHINK"
        if "cam:" in blob and "lidar:" in blob:
            return 0.1, "ACCEPT"
        return 0.15, "ACCEPT"


class _SafetyAndFuseGate:
    """Safe action passes; fusion after 'bad' reading yields high σ."""

    def score(self, prompt: str, response: str) -> tuple[float, str]:
        p, r = str(prompt), str(response)
        if "is this action safe?" in p:
            return 0.12, "ACCEPT"
        if "single sensor" in p:
            return 0.25, "ACCEPT"
        if "bad_after" in p or "bad_after" in r:
            return 0.88, "RETHINK"
        if "cam:" in p and "lidar:" in r:
            return 0.11, "ACCEPT"
        if "lidar:" in p and "cam:" in r:
            return 0.11, "ACCEPT"
        return 0.2, "ACCEPT"


class _ControlGate:
    def score(self, prompt: str, response: str) -> tuple[float, str]:
        p, r = str(prompt), str(response)
        if "is this action safe?" in p:
            return 0.1, "ACCEPT"
        if "single sensor" in p:
            if "after_step_" in r:
                return 0.75, "RETHINK"
            return 0.12, "ACCEPT"
        if "after_step_" in r or "after_step_" in p:
            return 0.75, "RETHINK"
        return 0.12, "ACCEPT"


def test_sensor_fusion_single() -> None:
    sf = SensorFusion(gate=_FixedSigma(0.22))
    sf.update("imu", "gyro_ok")
    out = sf.fuse()
    assert out["modalities"] == 1
    assert out["σ"] == 0.22
    assert out["conflict"] == []


def test_sensor_fusion_multi_consistent() -> None:
    sf = SensorFusion(gate=_PairConflictGate())
    sf.update("cam", "view_ahead_clear")
    sf.update("lidar", "no_obstacles")
    out = sf.fuse()
    assert out["modalities"] == 2
    assert out["σ"] < 0.3
    assert out["consistent"] is True
    assert out["conflict"] == []


def test_sensor_fusion_conflict() -> None:
    sf = SensorFusion(gate=_PairConflictGate())
    sf.update("cam", "ok")
    sf.update("lidar", "stale_reading")
    out = sf.fuse()
    assert out["consistent"] is False
    assert set(out["conflict"]) >= {"cam", "lidar"}


def test_act_safe_executes() -> None:
    c = EmbodiedController(gate=_FixedSigma(0.08, "ACCEPT"))
    r = c.act("move_forward 0.1m")
    assert r["executed"] is True
    assert r["σ_pre"] == 0.08
    assert len(c.action_history) == 1


def test_act_risky_abstains() -> None:
    c = EmbodiedController(gate=_FixedSigma(0.9, "ABSTAIN"))
    r = c.act("override motor interlock")
    assert r["executed"] is False
    assert "ABSTAIN" in r["reason"]


def test_close_loop_measures_error() -> None:
    c = EmbodiedController(gate=_SafetyAndFuseGate())
    c.observe({"cam": "ok", "lidar": "ok"})
    c.act("noop")
    cl = c.close_loop({"cam": "ok", "lidar": "bad_after"})
    assert cl["prediction_error"] > 0.2
    assert cl["correction_needed"] is True
    assert cl["action"] == "correct"


def test_control_loop_corrects() -> None:
    c = EmbodiedController(gate=_ControlGate())

    def sensor_fn(step):
        if step == 0.5:
            return {"read": "after_step_0"}
        if step == 1.5:
            return {"read": "after_step_1"}
        return {"read": f"step_{step}"}

    def action_fn(_obs, _sidx):
        return "forward"

    out = c.control_loop(sensor_fn, action_fn, n_steps=2)
    assert out["corrections"] >= 1
    assert out["steps"] == 2
    assert "results" in out
