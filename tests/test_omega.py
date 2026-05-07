# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.omega import OmegaLoop  # noqa: E402
from cos.sigma_gate import ABSTAIN  # noqa: E402


class _GateRethinkThenAccept:
    def __init__(self) -> None:
        self._n = 0

    def score(self, prompt: str, response: str):
        del prompt
        self._n += 1
        if "[rethink]" not in response:
            return 0.55, "RETHINK"
        return 0.05, "ACCEPT"


class _ConstGate:
    def __init__(self, sigma: float) -> None:
        self.sigma = float(sigma)

    def score(self, prompt: str, response: str):
        del prompt, response
        s = self.sigma
        if s < 0.15:
            return s, "ACCEPT"
        return s, ABSTAIN


def test_single_step_returns_sigma() -> None:
    out = OmegaLoop().step("hello world")
    assert "σ" in out
    assert isinstance(out["σ"], float)
    assert "verdict" in out


def test_abstain_on_high_sigma() -> None:
    # Entropy on (long repetitive prompt + Ω reasoning) is not guaranteed to land in ABSTAIN;
    # use an explicit high-σ gate to assert the abstain decision path.
    om = OmegaLoop(gate=_ConstGate(0.92))
    out = om.step("any prompt")
    assert out["verdict"] == ABSTAIN


def test_rethink_retries() -> None:
    om = OmegaLoop(gate=_GateRethinkThenAccept())
    out = om.step("question?")
    assert out["result"]["kind"] in ("rethink", "accept")
    assert len(om.σ_history) == 1


def test_history_accumulates() -> None:
    om = OmegaLoop()
    om.step("one")
    om.step("two")
    assert len(om.σ_history) == 2


def test_total_sigma_decreases_over_good_inputs() -> None:
    good = OmegaLoop(gate=_ConstGate(0.05))
    bad = OmegaLoop(gate=_ConstGate(0.92))
    good.run(["x", "y", "z"], max_steps=10)
    bad.run(["x", "y", "z"], max_steps=10)
    assert good.total_σ() < bad.total_σ()


def test_run_multiple_inputs() -> None:
    om = OmegaLoop()
    rows = om.run(["p1", "p2"], max_steps=10)
    assert len(rows) == 2
    assert all("σ" in r for r in rows)


def test_reflect_produces_sigma_meta() -> None:
    out = OmegaLoop().step("reflect ok")
    assert "σ_meta" in out
    assert isinstance(out["σ_meta"], float)


def test_world_model_populates_last_jepa() -> None:
    from cos.jepa import SigmaJEPA

    o = OmegaLoop(world_model=SigmaJEPA(dim=16))
    o.step("probe")
    assert o._last_jepa is not None
    assert "σ" in o._last_jepa
