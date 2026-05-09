# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

"""Tests for :mod:`cos.genesis.architect` — σ-scored discrete architecture search (lab)."""
from __future__ import annotations

import random
import sys
from pathlib import Path
from typing import Tuple

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.genesis.architect import ModuleSpec, SigmaArchitect  # noqa: E402


class _WinnerGate:
    """Low σ when ``L2:winner`` appears in the architecture description."""

    def score(self, prompt: str, response: str) -> Tuple[float, str]:
        _ = response
        if "L2:winner" in str(prompt):
            return 0.06, "ACCEPT"
        return 0.82, "RETHINK"


class _SwapGate:
    """First evaluation high σ, second lower (hot-swap acceptance)."""

    def __init__(self) -> None:
        self._n = 0

    def score(self, prompt: str, response: str) -> Tuple[float, str]:
        _ = prompt, response
        self._n += 1
        if self._n <= 2:
            return 0.7, "RETHINK"
        return 0.35, "RETHINK"


class _SpeedGate:
    """Prefer ``fast`` over ``slow`` in the serialized architecture string."""

    def score(self, prompt: str, response: str) -> Tuple[float, str]:
        _ = response
        p = str(prompt)
        if "L1:fast" in p:
            return 0.12, "ACCEPT"
        return 0.64, "RETHINK"


def test_define_search_space() -> None:
    a = SigmaArchitect(gate=_WinnerGate(), rng=random.Random(0))
    m = ModuleSpec("pack", ["c1"])
    assert m.name == "pack"
    a.define_search_space("L1", ["a", "b"])
    a.define_search_space("L2", ["x", "winner"])
    assert a.search_space["L2"] == ["x", "winner"]


def test_generate_candidate() -> None:
    a = SigmaArchitect(gate=_WinnerGate(), rng=random.Random(42))
    a.define_search_space("L1", ["p", "q"])
    a.define_search_space("L2", ["r", "s"])
    c = a.generate_candidate()
    assert set(c.keys()) == {"L1", "L2"}
    assert c["L1"] in ("p", "q")
    assert c["L2"] in ("r", "s")


def test_evaluate_returns_sigma() -> None:
    a = SigmaArchitect(gate=_WinnerGate(), rng=random.Random(0))
    a.define_search_space("L1", ["a"])
    a.define_search_space("L2", ["winner"])
    arch = {"L1": "a", "L2": "winner"}
    r = a.evaluate(arch, ["hello", "world"])
    assert "avg_σ" in r
    assert r["avg_σ"] < 0.2
    assert r["architecture"] == arch


def test_search_finds_best() -> None:
    a = SigmaArchitect(gate=_WinnerGate(), rng=random.Random(123))
    a.define_search_space("L1", ["a", "b", "c"])
    a.define_search_space("L2", ["x", "y", "winner"])
    out = a.search(["probe"], n_candidates=12, n_generations=8, converge_threshold=0.1)
    assert out["best_architecture"] is not None
    assert out["best_architecture"].get("L2") == "winner"
    assert out["best_σ"] < 0.15


def test_search_sigma_decreases() -> None:
    a = SigmaArchitect(gate=_WinnerGate(), rng=random.Random(7))
    a.define_search_space("L1", ["a", "b"])
    a.define_search_space("L2", ["x", "winner"])
    out = a.search(["t1", "t2"], n_candidates=10, n_generations=6, converge_threshold=0.15)
    assert out["converged"] is True
    assert out["best_σ"] < 0.15


def test_hot_swap_validates() -> None:
    a = SigmaArchitect(gate=_SwapGate(), rng=random.Random(0))
    a.define_search_space("L1", ["slow", "fast"])
    cur = {"L1": "slow"}
    # 2 prompts * first eval = 2 scores at 0.7 -> avg 0.7; swap eval 2 at 0.35 -> avg 0.35
    rep = a.hot_swap(cur, "L1", "fast", ["p1", "p2"])
    assert rep["σ_before"] == 0.7
    assert rep["σ_after"] == 0.35
    assert rep["accepted"] is True


def test_auto_optimize_improves() -> None:
    a = SigmaArchitect(gate=_SpeedGate(), rng=random.Random(1))
    a.define_search_space("L1", ["slow", "fast"])
    cur = {"L1": "slow"}
    out = a.auto_optimize(cur, ["check"])
    assert out["optimized"]["L1"] == "fast"
    assert out["n_improved"] == 1
    assert out["final_σ"] < 0.2

