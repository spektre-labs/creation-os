# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.dynamic_bench`."""
from __future__ import annotations

from cos.dynamic_bench import SigmaDynamicBench
from cos.sigma_gate import SigmaGate


def test_generate_questions_tagged() -> None:
    b = SigmaDynamicBench()
    qs = b.generate_questions("math", "hard", 2)
    assert len(qs) == 2
    assert "dynamic-bench" in qs[0]["question"]


def test_sigma_validate_questions() -> None:
    b = SigmaDynamicBench()
    g = SigmaGate()
    qs = b.generate_questions("x", "easy", 1)
    v = b.sigma_validate_questions(qs, g)
    assert v["n"] == 1 and "items" in v


def test_run_m_tiers() -> None:
    b = SigmaDynamicBench()
    g = SigmaGate()

    class Ans:
        def answer(self, q: str) -> str:
            return "42"

    qs = b.generate_questions("g", "easy", 2)
    r = b.run(Ans(), g, qs)
    assert sum(r["m_tier_counts"].values()) == 2


def test_compare_static_vs_dynamic_shape() -> None:
    b = SigmaDynamicBench()
    g = SigmaGate()

    class Ans:
        def answer(self, q: str) -> str:
            return q[:20]

    c = b.compare_static_vs_dynamic(Ans(), g)
    assert "static_pool_run" in c and "dynamic_pool_run" in c


def test_snapshots_and_gradient() -> None:
    b = SigmaDynamicBench()
    g = SigmaGate()

    class Ans:
        def answer(self, q: str) -> str:
            return "ok"

    qs = b.generate_questions("bio", "medium", 1)
    snap = b.versioned_snapshots(qs)
    assert snap["version"] >= 1
    grad = b.difficulty_gradient(Ans(), g, domain="bio")
    assert len(grad["curve"]) == 4
