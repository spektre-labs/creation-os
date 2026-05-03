# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import tempfile
from pathlib import Path

from cos.recursion import SigmaRecursion
from cos.sigma_gate import SigmaGate


def test_route_simple_tokens_shallow() -> None:
    gate = SigmaGate()
    sr = SigmaRecursion(max_depth=8)
    route = sr.token_route(["a", "b", "c"], gate, context="ctx")
    assert len(route["depths"]) == 3
    assert max(route["depths"]) <= 8
    assert min(route["depths"]) >= 1


def test_route_complex_tokens_deep() -> None:
    gate = SigmaGate()
    shallow = SigmaRecursion(max_depth=8)
    deep_probe = SigmaRecursion(max_depth=8)
    simple = shallow.token_route(["x", "x", "x"], gate, context="q")
    messy = deep_probe.token_route(
        ["Z", "9", "!", "Q", "µ", "∇", "⊗", "Ω"],
        gate,
        context="reason about symplectic morphisms",
    )
    assert messy["mean_depth"] >= simple["mean_depth"]


def test_selective_kv_smaller_than_full() -> None:
    sr = SigmaRecursion(max_depth=8)
    toks = ["a", "b", "c"]
    depths = [1, 2, 3]
    kv = sr.selective_kv_cache(toks, depths, max_depth=8)
    assert kv["active_kv_slots"] < kv["full_kv_slots"]


def test_halt_on_sigma_stable() -> None:
    gate = SigmaGate()
    sr = SigmaRecursion(halt_epsilon=0.0001)
    out = sr.halt_on_sigma_stable(
        "verify",
        "seed",
        gate,
        lambda _: "constant_answer",
        max_rounds=8,
    )
    assert out["halted"] is True
    assert out["rounds"] >= 2


def test_compute_savings() -> None:
    sr = SigmaRecursion(max_depth=8)
    savings = sr.compute_savings(["a", "b", "c"], [1, 2, 3], max_depth=8)
    assert savings["savings_ratio"] > 0.0


def test_deterministic() -> None:
    gate = SigmaGate()
    sr = SigmaRecursion(max_depth=6)
    toks = ["why", "σ", "depth"]
    r1 = sr.token_route(toks, gate, context="prompt")
    r2 = sr.token_route(toks, gate, context="prompt")
    assert r1["depths"] == r2["depths"]
    assert r1["sigmas"] == r2["sigmas"]


def test_per_token_sigma_trace() -> None:
    gate = SigmaGate()
    sr = SigmaRecursion()
    toks = ["one", "two", "three"]
    route = sr.token_route(toks, gate, context="c")
    assert len(route["per_token"]) == len(toks)
    assert all("sigma" in row and "depth" in row for row in route["per_token"])


def test_fabric_integration() -> None:
    from cos.fabric import SigmaFabric

    fabric = SigmaFabric(snapshot_dir=Path(tempfile.mkdtemp(prefix="cos_rec_")))
    fabric.boot()
    assert "recursion" in fabric.layers
    result = fabric.process("What is σ?", response="depth adapts per token here")
    step_layers = [s["layer"] for s in result.trace.to_dict()["steps"]]
    assert "recursion" in step_layers
