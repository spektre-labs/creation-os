# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.predictive import PredictiveCoding, PredictiveLayer  # noqa: E402


class _ConstGate:
    def __init__(self, sigma: float) -> None:
        self._s = float(sigma)

    def score(self, a: str, b: str):  # noqa: ARG002
        return (self._s, "ACCEPT")


class _SubstringGate:
    """Lower σ when one string is a substring of the other (after updates)."""

    def score(self, a: str, b: str):  # noqa: ARG002
        sa, sb = str(a), str(b)
        if sa in sb or sb in sa or not sa or not sb:
            return (0.12, "ACCEPT")
        return (0.72, "ACCEPT")


def test_predict_sets_prediction() -> None:
    g = _ConstGate(0.3)
    pl = PredictiveLayer("L0", 0, g)
    pl.predict("state")
    assert pl.prediction == "state"


def test_observe_computes_error() -> None:
    g = _ConstGate(0.4)
    pl = PredictiveLayer("L0", 0, g)
    pl.predict("x")
    e = pl.observe("y")
    assert abs(e - 0.4) < 1e-9


def test_top_down_flows_down() -> None:
    pc = PredictiveCoding(gate=_ConstGate(0.2), n_levels=3)
    pc.top_down_pass("goal")
    assert all(L.prediction is not None for L in pc.layers)


def test_bottom_up_flows_up() -> None:
    pc = PredictiveCoding(gate=_ConstGate(0.25), n_levels=3)
    pc.top_down_pass("prior")
    errs = pc.bottom_up_pass("sense")
    assert len(errs) == 3
    assert all("σ" in e for e in errs)


def test_full_cycle_returns_sigma() -> None:
    pc = PredictiveCoding(gate=_ConstGate(0.35), n_levels=4)
    r = pc.full_cycle("data", high_level_state="prior")
    assert r["layers"] == 4
    assert "total_σ" in r
    assert len(r["errors"]) == 4


def test_run_reduces_sigma() -> None:
    pc = PredictiveCoding(gate=_SubstringGate(), n_levels=3)
    brief = pc.run(["hello there"], n_iterations=2, high_level_state="hello")
    more = pc.run(["hello there"], n_iterations=18, high_level_state=None)
    assert more["σ_trajectory"][0] < brief["σ_trajectory"][0]


def test_precision_weighting() -> None:
    pc = PredictiveCoding(gate=_ConstGate(0.5), n_levels=2)
    pc.precision_weight(0, 0.5)
    pc.layers[0].predict("p")
    e = pc.layers[0].observe("o")
    assert abs(e - 0.25) < 1e-9
