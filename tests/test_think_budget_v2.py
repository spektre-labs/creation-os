# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

"""V2 :mod:`cos.think_budget` — σ-weighted allocation and under/over-think heuristics."""
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
        _ = prompt
        _ = response
        j = min(self._i, len(self._values) - 1)
        v = float(self._values[j])
        self._i += 1
        return v, "RETHINK"


def test_coda_allocate_proportional() -> None:
    tb = ThinkBudget(gate=_ListGate([0.25, 0.75]), max_tokens=100)
    out = tb.coda_allocate("root", ["chunk-a", "chunk-b"])
    totals = sum(a["tokens_allocated"] for a in out["allocations"])
    assert totals == 100
    assert abs(sum(a["fraction"] for a in out["allocations"]) - 1.0) < 1e-6
    fr = {a["sub_question"]: a["tokens_allocated"] for a in out["allocations"]}
    assert fr["chunk-b"] > fr["chunk-a"]


def test_easy_gets_less_tokens() -> None:
    tb = ThinkBudget(gate=_ListGate([0.1, 0.9]), max_tokens=1000)
    out = tb.coda_allocate("q", ["easy_like", "hard_like"])
    by_sq = {a["sub_question"]: a["tokens_allocated"] for a in out["allocations"]}
    assert by_sq["easy_like"] < by_sq["hard_like"]


def test_hard_gets_more_tokens() -> None:
    tb = ThinkBudget(gate=_ListGate([0.2, 0.8]), max_tokens=200)
    out = tb.coda_allocate("q", ["low_sigma", "high_sigma"])
    by_sq = {a["sub_question"]: a["tokens_allocated"] for a in out["allocations"]}
    assert by_sq["high_sigma"] > by_sq["low_sigma"]


def test_plan_and_budget_decomposes() -> None:
    def plan_fn(p: str) -> List[str]:
        return [f"{p}:sub1", f"{p}:sub2"]

    tb = ThinkBudget(
        gate=_ListGate([0.4, 0.6, 0.35, 0.65]),
        max_tokens=50,
    )
    out = tb.plan_and_budget("problem", plan_fn=plan_fn)
    assert out["plan"] == ["problem:sub1", "problem:sub2"]
    assert len(out["results"]) == 2
    assert "budget" in out and "allocations" in out["budget"]
    assert out["total_σ"] == round((out["results"][0]["σ"] + out["results"][1]["σ"]) / 2, 4)


def test_underthink_overthink_detected() -> None:
    tb = ThinkBudget()
    rep = tb.underthink_overthink_detector([0.55, 0.08], [80, 600])
    assert rep.get("healthy") is False
    assert rep["underthinking"] == 1
    assert rep["overthinking"] == 1
    assert rep["tokens_wasted"] == 600
