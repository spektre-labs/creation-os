# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from cos.kv_cache import SigmaKVCache


def test_put_evict_high_sigma() -> None:
    c = SigmaKVCache(max_size=3)
    c.put("a", 0.1)
    c.put("b", 0.9)
    c.put("c", 0.85)
    c.put("d", 0.2)
    assert len(c) <= 3


def test_budget() -> None:
    c = SigmaKVCache(max_size=5)
    for i in range(10):
        c.put(str(i), float(i) / 10.0)
    assert len(c) == 5


def test_sliding_prefers_low_sigma() -> None:
    c = SigmaKVCache(max_size=8)
    for i in range(6):
        c.put(f"k{i}", 0.1 + i * 0.1)
    c.sliding_window(3, keep_below=0.25)
    assert len(c) <= 8


def test_merge_similar() -> None:
    c = SigmaKVCache(max_size=20)
    c.put("t:0", 0.11)
    c.put("t:1", 0.12)
    removed = c.merge_similar(sigma_quant=0.05)
    assert removed >= 0
    assert len(c) >= 1


def test_stats() -> None:
    c = SigmaKVCache()
    c.put("x", 0.3)
    s = c.stats()
    assert s["entries"] == 1


def test_get_miss() -> None:
    c = SigmaKVCache()
    assert c.get("nope") is None
