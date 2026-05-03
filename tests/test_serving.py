# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for ``cos.serving``."""
from __future__ import annotations

from cos.sigma_gate import SigmaGate
from cos.serving import SigmaServing, ThroughputMonitor


def test_vllm_and_sglang_hooks_score() -> None:
    g = SigmaGate()
    s = SigmaServing()
    v = s.vllm_hook(None, g)
    assert v["after_generation"]("p", "r") == g.score("p", "r")
    sg = s.sglang_hook(object(), g)
    assert sg["after_emit"]("1", "2")[1] in ("ACCEPT", "RETHINK", "ABSTAIN")


def test_batch_score_order_preserved() -> None:
    g = SigmaGate()
    s = SigmaServing()
    rows = [{"prompt": "a", "response": "1"}, {"prompt": "b", "response": "2"}]
    out = s.batch_score(rows, g)
    assert len(out) == 2 and out[0]["prompt"] == "a" and out[1]["prompt"] == "b"


def test_prefix_cache_sigma_hit() -> None:
    g = SigmaGate()
    s = SigmaServing()
    r1 = s.prefix_cache_sigma("hello", g)
    r2 = s.prefix_cache_sigma("hello", g)
    assert r1["cache_hit"] is False and r2["cache_hit"] is True
    assert r1["key"] == r2["key"]


def test_throughput_monitor_snapshot() -> None:
    m = ThroughputMonitor(window_seconds=10.0)
    m.record(tokens=100, sigma_evals=2)
    snap = m.snapshot()
    assert snap["tokens_per_s"] >= 0 and snap["sigma_evals_per_s"] >= 0


def test_sigma_aware_scheduling_partitions() -> None:
    g = SigmaGate()
    s = SigmaServing()
    r = s.sigma_aware_scheduling(
        [
            {"prompt": "hello", "response": "world"},
            {"prompt": "explain", "response": ""},
        ],
        g,
        sigma_fast_threshold=0.35,
    )
    assert r["n_fast"] + r["n_deep"] == 2
    assert r["n_deep"] >= 1


def test_sigma_serving_shares_named_monitor() -> None:
    s = SigmaServing()
    m1 = s.throughput_monitor(30.0, name="lane_a")
    m2 = s.throughput_monitor(30.0, name="lane_a")
    assert m1 is m2
