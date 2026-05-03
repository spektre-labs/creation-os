# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for ``cos.parallel``."""
from __future__ import annotations

from cos.parallel import SigmaParallel
from cos.sigma_gate import SigmaGate


def test_tensor_and_pipeline_plans() -> None:
    p = SigmaParallel()
    tp = p.tensor_parallel(None, 4)
    pp = p.pipeline_parallel({}, 2)
    assert tp["n_gpus"] == 4 and pp["stages"] == 2


def test_sigma_per_shard() -> None:
    agg = SigmaParallel.sigma_per_shard([0.1, 0.6, 0.2])
    assert agg["max"] == 0.6 and abs(agg["mean"] - 0.3) < 1e-6


def test_load_balance_orders_by_pressure() -> None:
    g = SigmaGate()
    shards = [{"id": "a", "load": 1.0, "sigma": 0.9}, {"id": "b", "load": 1.0, "sigma": 0.2}]
    order = SigmaParallel.load_balance(shards, g)["order"]
    assert order[0]["id"] == "b"


def test_communication_overhead_positive() -> None:
    o = SigmaParallel.communication_overhead({"edges": 4, "bytes_per_step": 1e7, "link_gbps": 25.0})
    assert o["estimated_sync_ms"] >= 0


def test_hybrid_parallel_world_size() -> None:
    h = SigmaParallel.hybrid_parallel(None, 2, 3)
    assert h["world_size"] == 6
