# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

"""Tests for :mod:`cos.strange_loop` — bounded σ tower + lab metaphors."""
from __future__ import annotations

import sys
from pathlib import Path
from typing import Tuple

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.strange_loop import StrangeLoop  # noqa: E402


class _ConstGate:
    def __init__(self, σ: float = 0.42) -> None:
        self._σ = float(σ)

    def score(self, prompt: str, response: str) -> Tuple[float, str]:
        _ = prompt, response
        return self._σ, "ACCEPT"


class _RampGate:
    """First scores differ, then plateau for fixed-point finder."""

    def __init__(self) -> None:
        self._i = 0
        self._vals = [0.5, 0.52, 0.52, 0.52, 0.52]

    def score(self, prompt: str, response: str) -> Tuple[float, str]:
        _ = prompt, response
        v = self._vals[min(self._i, len(self._vals) - 1)]
        self._i += 1
        return v, "ACCEPT"


def test_recurse_produces_trace() -> None:
    sl = StrangeLoop(gate=_ConstGate(0.33), max_depth=4)
    out = sl.recurse("seed text")
    assert len(out["trace"]) == 4
    assert out["depth"] == 4
    assert all("σ" in row and "verdict" in row for row in out["trace"])


def test_fixed_point_found() -> None:
    sl = StrangeLoop(gate=_RampGate(), max_depth=6)
    out = sl.recurse("x")
    assert out["fixed_point"] is not None
    assert "σ_fixed" in out["fixed_point"]
    assert out["fixed_point"]["depth"] >= 0


def test_loop_closes() -> None:
    sl = StrangeLoop(gate=_ConstGate(0.25), max_depth=5)
    out = sl.recurse("stable")
    assert out["loop_closed"] is True
    assert out["identity_emerged"] is True


def test_tangled_hierarchy() -> None:
    sl = StrangeLoop(gate=_ConstGate(0.4), max_depth=2)
    h = sl.tangled_hierarchy()
    assert "layers" in h and len(h["layers"]) == 10
    assert "σ_top_down" in h and "σ_bottom_up" in h
    assert h["tangled"] is True


def test_godel_meets_hofstadter() -> None:
    sl = StrangeLoop(gate=_ConstGate(0.31), max_depth=1)
    g = sl.godel_meets_hofstadter()
    assert "godel" in g and "hofstadter" in g
    assert "σ_self" in g and "resolution" in g


def test_max_depth_bounded() -> None:
    sl = StrangeLoop(gate=_ConstGate(), max_depth=1000)
    assert sl.max_depth == 32
    out = sl.recurse("cap")
    assert len(out["trace"]) == 32
