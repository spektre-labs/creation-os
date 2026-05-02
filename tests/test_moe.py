# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from cos.moe import SigmaMoE


def test_add_expert() -> None:
    moe = SigmaMoE()
    moe.add_expert("fast", cost=0.1)
    moe.add_expert("deep", cost=1.0)
    assert len(moe.experts) == 2


def test_cascade_score_only() -> None:
    moe = SigmaMoE()
    moe.add_expert("fast", cost=0.1)
    moe.add_expert("deep", cost=1.0)
    result = moe.route("test", response="hello", strategy="cascade")
    assert "sigma" in result
    assert "expert" in result
    assert "trace" in result


def test_parallel() -> None:
    moe = SigmaMoE()
    moe.add_expert("a", cost=0.5)
    moe.add_expert("b", cost=1.0)
    result = moe.route("test", response="hello", strategy="parallel")
    assert result.get("expert") is not None


def test_budget_aware() -> None:
    moe = SigmaMoE()
    moe.add_expert("cheap", cost=0.1)
    moe.add_expert("medium", cost=0.5)
    moe.add_expert("expensive", cost=2.0)
    result = moe.route("test", response="hello", strategy="budget", budget=0.7)
    used = [t["expert"] for t in result["trace"]]
    assert "expensive" not in used


def test_sigma_range_filter() -> None:
    moe = SigmaMoE()
    moe.add_expert("low_only", cost=0.1, max_sigma=0.3)
    moe.add_expert("high_only", cost=1.0, min_sigma=0.5)
    result = moe.route("test", response="hello", sigma=0.7, strategy="cascade")
    used = [t["expert"] for t in result["trace"]]
    assert "low_only" not in used


def test_expert_stats() -> None:
    moe = SigmaMoE()
    moe.add_expert("a", cost=0.5)
    moe.route("test", response="hello")
    moe.route("test2", response="world")
    stats = moe.expert_stats()
    assert "a" in stats
    assert stats["a"]["calls"] >= 1


def test_routing_signature() -> None:
    moe = SigmaMoE()
    moe.add_expert("fast", cost=0.1)
    moe.add_expert("deep", cost=1.0)
    for _ in range(5):
        moe.route("test", response="hello")
    sig = moe.routing_signature()
    assert isinstance(sig, dict)
    assert sum(sig.values()) <= 1.01


def test_no_expert_match() -> None:
    moe = SigmaMoE()
    moe.add_expert("only_low", cost=0.1, max_sigma=0.1)
    result = moe.route("test", response="hello", sigma=0.9, strategy="cascade")
    assert result["expert"] is None
    assert result["verdict"] == "NO_EXPERT"
    assert result["trace"] == []
