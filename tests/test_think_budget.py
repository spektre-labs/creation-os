# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

"""Tests for :mod:`cos.think_budget` — σ depth search and branch policy."""
from __future__ import annotations

import sys
from pathlib import Path
from typing import List, Tuple

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.think_budget import ThinkBudget  # noqa: E402


class _ListGate:
    def __init__(self, values: List[float]) -> None:
        self._values = values
        self._i = 0

    def score(self, prompt: str, response: str) -> Tuple[float, str]:
        _ = prompt, response
        j = min(self._i, len(self._values) - 1)
        v = float(self._values[j])
        self._i += 1
        return v, "ACCEPT"


class _ConstGate:
    def __init__(self, σ: float) -> None:
        self._σ = float(σ)

    def score(self, prompt: str, response: str) -> Tuple[float, str]:
        _ = prompt, response
        return self._σ, "ACCEPT"


def test_optimal_depth_finds_minimum() -> None:
    # σ improves then worsens with three consecutive rises → early exit
    tb = ThinkBudget(gate=_ListGate([0.5, 0.4, 0.35, 0.45, 0.55, 0.65]), max_tokens=800)
    out = tb.optimal_depth(
        "p",
        think_fn=lambda pr, depth=0: f"thought {depth}",
        max_steps=10,
    )
    assert out["best_σ"] == 0.35
    assert out["best_step"] == 2
    assert len(out["σ_trace"]) == 5


def test_overthinking_detected() -> None:
    tb = ThinkBudget(gate=_ListGate([0.3, 0.35, 0.4, 0.5]), max_tokens=512)
    out = tb.optimal_depth("q", think_fn=lambda pr, depth=0: str(depth), max_steps=8)
    assert out["total_steps"] == 3
    assert out["overthinking_detected"] is True


def test_branch_confident_no_branch() -> None:
    tb = ThinkBudget(gate=_ConstGate(0.08))
    b = tb.branch_or_not("a", "b")
    assert b["branch"] is False
    assert b["σ"] == 0.08


def test_branch_uncertain_branches() -> None:
    tb = ThinkBudget(gate=_ConstGate(0.55))
    b = tb.branch_or_not("a", "b")
    assert b["branch"] is True
    assert b["n_paths"] == 3


def test_marginal_gain_stop() -> None:
    tb = ThinkBudget()
    m = tb.marginal_gain([0.5, 0.42, 0.41, 0.405])
    assert m["diminishing_returns"] is True
    assert m["keep_thinking"] is False


def test_compute_budget_scales() -> None:
    tb = ThinkBudget(gate=_ConstGate(0.12))
    b = tb.compute_budget("easy?")
    assert b["tokens"] == 256
    assert b["paths"] == 1

    tb2 = ThinkBudget(gate=_ConstGate(0.85))
    b2 = tb2.compute_budget("hard?")
    assert b2["tokens"] == 4096
    assert b2["paths"] == 5
