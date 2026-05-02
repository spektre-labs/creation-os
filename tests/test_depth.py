# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from cos.depth import AdaptiveDepth, DepthResult, DepthRouter


def test_score_depth_basic() -> None:
    depth = AdaptiveDepth()
    responses = [
        "I think the answer might be 4",
        "The answer is 4 because 2+2=4",
        "2+2=4. This follows from Peano axioms.",
    ]
    result = depth.score_depth("What is 2+2?", responses)
    assert isinstance(result, DepthResult)
    assert result.loops_used <= 3
    assert 0 <= result.sigma <= 1


def test_depth_halts_on_good_answer() -> None:
    depth = AdaptiveDepth(halt_threshold=0.5)
    responses = ["4", "4", "4", "4"]
    result = depth.score_depth("What is 2+2?", responses)
    assert result.loops_used <= len(responses)


def test_depth_result_bool() -> None:
    r = DepthResult("ok", 0.1, "ACCEPT", 2, 8, [], 5.0, "2/8 loops used")
    assert bool(r) is True
    r2 = DepthResult(None, 0.9, "ABSTAIN", 8, 8, [], 50.0, "8/8 loops used")
    assert bool(r2) is False


def test_depth_trace() -> None:
    depth = AdaptiveDepth()
    responses = ["a", "b", "c"]
    result = depth.score_depth("test", responses)
    assert len(result.trace) > 0
    assert "sigma" in result.trace[0]
    assert "delta" in result.trace[0]


def test_router_simple_question() -> None:
    router = DepthRouter()
    loops = router.allocate("What is 2+2?")
    assert loops <= 2


def test_router_complex_question() -> None:
    router = DepthRouter()
    loops = router.allocate(
        "Prove that the square root of 2 is irrational "
        "using proof by contradiction and explain each step"
    )
    assert loops >= 4


def test_router_math_domain() -> None:
    router = DepthRouter()
    loops = router.allocate("Derive the theorem for category theory morphisms")
    assert loops >= 3


def test_compute_saved() -> None:
    depth = AdaptiveDepth(max_loops=8)
    responses = ["good answer"] * 8
    result = depth.score_depth("test", responses)
    assert "loops used" in result.compute_saved
