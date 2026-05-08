# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

"""Tests for :mod:`cos.autonomous` — σ loop, drift, convergence halt."""
from __future__ import annotations

import sys
from pathlib import Path
from typing import Any, Tuple

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.autonomous import AutonomousAgent  # noqa: E402


class _LowSigmaGate:
    def score(self, prompt: str, response: str) -> Tuple[float, str]:
        _ = prompt, response
        return 0.08, "ACCEPT"


class _HighPlateauGate:
    def score(self, prompt: str, response: str) -> Tuple[float, str]:
        _ = prompt, response
        return 0.78, "ACCEPT"


class _MidSigmaGate:
    def score(self, prompt: str, response: str) -> Tuple[float, str]:
        _ = prompt, response
        return 0.35, "ACCEPT"


class _DriftGate:
    def score(self, prompt: str, response: str) -> Tuple[float, str]:
        if str(prompt) == "goal clarity":
            return 0.05, "ACCEPT"
        return 0.88, "ACCEPT"


def test_set_goal() -> None:
    a = AutonomousAgent(gate=_LowSigmaGate(), max_steps=10)
    a.set_goal("ship feature X")
    assert a.goal == "ship feature X"
    assert a.goal_σ is not None
    assert a.start_time is not None
    assert len(a.log) == 0


def test_step_returns_decision() -> None:
    a = AutonomousAgent(gate=_LowSigmaGate(), max_steps=5)
    a.set_goal("g")

    def act(_g: Any, _c: str, s: int) -> str:
        return f"ok {s}"

    e = a.step(act, "")
    assert e["step"] == 1
    assert "decision" in e and "σ" in e


def test_run_completes() -> None:
    a = AutonomousAgent(gate=_LowSigmaGate(), max_steps=30, conv_window=5)
    a.set_goal("finish proof")

    def act(_g: Any, _c: str, s: int) -> str:
        return f"progress {s}"

    out = a.run(act)
    assert out["reason"] == "completed"
    assert out["steps"] >= 5


def test_loop_detected_halts() -> None:
    a = AutonomousAgent(gate=_HighPlateauGate(), max_steps=30, conv_window=5)
    a.set_goal("loop task")

    def act(_g: Any, _c: str, s: int) -> str:
        return f"stuck {s}"

    out = a.run(act)
    assert out["reason"] == "HALT_LOOP"


def test_drift_triggers_correction() -> None:
    a = AutonomousAgent(gate=_DriftGate(), max_steps=10, drift_threshold=0.2)
    a.set_goal("stable spec")

    def act(_g: Any, _c: str, s: int) -> str:
        return "off-spec ramble" if s == 1 else "closer"

    e = a.step(act, "")
    assert e["drifted"] is True
    assert e["decision"] == "SELF_CORRECT_DRIFT"


def test_timeout_halts(monkeypatch: Any) -> None:
    import cos.autonomous as aut_mod

    clock = {"t": 0.0}

    def fake_time() -> float:
        clock["t"] += 1000.0
        return clock["t"]

    monkeypatch.setattr(aut_mod.time, "time", fake_time)
    a = AutonomousAgent(gate=_MidSigmaGate(), max_steps=100, timeout_s=50.0)
    a.set_goal("t")

    def act(_g: Any, _c: str, s: int) -> str:
        return str(s)

    out = a.run(act)
    assert out["reason"] == "HALT_TIMEOUT"


def test_max_steps_halts() -> None:
    a = AutonomousAgent(gate=_MidSigmaGate(), max_steps=4)
    a.set_goal("cap")

    def act(_g: Any, _c: str, s: int) -> str:
        return str(s)

    out = a.run(act)
    assert out["reason"] == "HALT_MAX_STEPS"
    assert out["steps"] == 4
