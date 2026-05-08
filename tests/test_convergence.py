# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import pytest

from cos.convergence import SigmaConvergence


def test_converged_low_sigma() -> None:
    c = SigmaConvergence(window=5, stall_threshold=0.01)
    c.begin()
    for _ in range(5):
        c.record(0.12)
    d = c.check()
    assert d["status"] == "CONVERGED"
    assert d["σ"] == 0.12


def test_loop_detected_stalled_high() -> None:
    c = SigmaConvergence(window=5, stall_threshold=0.02)
    c.begin()
    for s in (0.62, 0.61, 0.62, 0.61, 0.62):
        c.record(s)
    d = c.check()
    assert d["status"] == "LOOP"
    assert d["Δσ"] < 0.02


def test_oscillating_detected() -> None:
    c = SigmaConvergence(window=5, stall_threshold=0.001)
    c.begin()
    for s in (0.35, 0.65, 0.35, 0.65, 0.35):
        c.record(s)
    d = c.check()
    assert d["status"] == "OSCILLATING"
    assert d.get("sign_changes", 0) >= 2


def test_max_turns_halts() -> None:
    c = SigmaConvergence(window=3, max_turns=4)
    c.begin()
    for s in (0.4, 0.5, 0.45, 0.48):
        c.record(s)
    d = c.check()
    assert d["status"] == "HALT"
    assert d["reason"] == "max_turns exceeded"


def test_timeout_halts(monkeypatch: pytest.MonkeyPatch) -> None:
    fixed: dict[str, float] = {"t": 1000.0}

    def fake_time() -> float:
        return fixed["t"]

    monkeypatch.setattr("cos.convergence.time.time", fake_time)
    c = SigmaConvergence(window=2, timeout_s=30.0)
    c.begin()
    fixed["t"] = 1100.0
    c.record(0.3)
    d = c.check()
    assert d["status"] == "HALT"
    assert d["reason"] == "timeout"


def test_continue_when_improving() -> None:
    c = SigmaConvergence(window=5, stall_threshold=0.001)
    c.begin()
    for s in (0.9, 0.7, 0.5, 0.3, 0.1):
        c.record(s)
    d = c.check()
    assert d["status"] == "CONTINUE"
    assert d.get("trend") == "improving"


def test_wrap_loop_auto_halts() -> None:
    c = SigmaConvergence(window=3, stall_threshold=0.01, max_turns=20)
    state = {"v": 0.7}

    def step() -> float:
        return state["v"]

    out = c.wrap_loop(step, max_steps=12)
    finals = [r.get("final") for r in out if isinstance(r, dict) and "final" in r]
    assert any(isinstance(f, dict) and f.get("status") == "LOOP" for f in finals)


def test_insufficient_data_continues() -> None:
    c = SigmaConvergence(window=5)
    c.begin()
    c.record(0.4)
    c.record(0.5)
    d = c.check()
    assert d["status"] == "CONTINUE"
    assert d["reason"] == "insufficient data"
