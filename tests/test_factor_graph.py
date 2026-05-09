# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.theory.factor_graph import Factor, SigmaFactorGraph, Variable  # noqa: E402


class _ConstGate:
    def __init__(self, sigma: float) -> None:
        self._s = float(sigma)

    def score(self, a: str, b: str):  # noqa: ARG002
        return (self._s, "ACCEPT")


class _MuBGate:
    """σ tracks numeric μb suffix in ``b`` (for belief-coupled VFE tests)."""

    def score(self, a: str, b: str):  # noqa: ARG002
        s = str(b)
        if "μb=" in s:
            try:
                tail = s.split("μb=", 1)[1].split()[0]
                mb = float(tail)
                return (min(0.95, max(0.05, mb)), "ACCEPT")
            except ValueError:
                pass
        return (0.5, "ACCEPT")


class _ActPrefGate:
    def score(self, a: str, b: str):  # noqa: ARG002
        if "good" in str(b):
            return (0.15, "ACCEPT")
        return (0.85, "ACCEPT")


def test_add_variable() -> None:
    fg = SigmaFactorGraph(gate=_ConstGate(0.3))
    v = fg.add_variable("x", 1)
    assert v.name == "x"
    assert "x" in fg.variables


def test_add_factor() -> None:
    fg = SigmaFactorGraph(gate=_ConstGate(0.4))
    fg.add_variable("a", "1")
    fg.add_variable("b", "2")
    f = fg.add_factor("pair", ["a", "b"])
    assert f.name == "pair"
    assert len(fg.factors) == 1


def test_evaluate_factor_returns_sigma() -> None:
    g = _ConstGate(0.33)
    v1 = Variable("a", "x")
    v2 = Variable("b", "y")
    fac = Factor("f", g, [v1, v2])
    s = fac.evaluate()
    assert abs(s - 0.33) < 1e-9


def test_message_passing_updates_beliefs() -> None:
    fg = SigmaFactorGraph(gate=_ConstGate(0.4))
    fg.add_variable("p", "hello")
    fg.add_variable("q", "world")
    fg.add_factor("link", ["p", "q"])
    fg.factors["link"].send_messages()
    fg.variables["p"].update_belief()
    assert fg.variables["p"].belief != 0.5


def test_infer_converges() -> None:
    fg = SigmaFactorGraph(gate=_ConstGate(0.25))
    fg.add_variable("u", "a")
    fg.add_variable("v", "b")
    fg.add_factor("uv", ["u", "v"])
    out = fg.infer(max_iterations=25, tolerance=0.001)
    assert out["converged"] is True
    assert out["iterations"] >= 2


def test_vfe_decreases() -> None:
    fg = SigmaFactorGraph(gate=_MuBGate())
    fg.add_variable("x", "1")
    fg.add_variable("y", "2")
    fg.add_factor("xy", ["x", "y"])
    out = fg.infer(max_iterations=30, tolerance=0.0001)
    hist = out["vfe_history"]
    assert hist[-1] <= hist[0] + 0.01


def test_observe_clamps_variable() -> None:
    fg = SigmaFactorGraph(gate=_ConstGate(0.5))
    fg.add_variable("z", None)
    fg.observe("z", "fixed")
    assert fg.variables["z"].value == "fixed"
    assert fg.variables["z"].belief == 0.0


def test_active_infer_selects_best() -> None:
    fg = SigmaFactorGraph(gate=_ActPrefGate())
    fg.add_variable("obs", "ctx")
    fg.add_variable("act", "")
    fg.add_factor("step", ["obs", "act"])
    out = fg.active_infer(["bad_action", "good_action"], "act")
    assert out["best_action"] == "good_action"
    assert out["best_vfe"] <= out["all"][-1]["expected_vfe"]
