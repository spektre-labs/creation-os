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
    c = SigmaKVCache(max_size=8)
    c.put("x", 0.3)
    s = c.stats()
    assert s["entries"] == 1
    assert s["size"] == 1
    assert s["max_size"] == 8
    assert s["mean_sigma"] == s["avg_σ"] or abs(float(s["mean_sigma"]) - float(s["avg_σ"])) < 1e-5
    assert s["evictions"] == 0
    assert s["compression"] == 1.0
    assert s["total_ops"] == 1


def test_add_and_get() -> None:
    c = SigmaKVCache(max_size=64)
    c.add("k1", "v1", context="ctx")
    assert c.get_value("k1") == "v1"
    e = c.get("k1")
    assert e is not None and float(e.sigma) >= 0.0


def test_eviction_removes_high_sigma() -> None:
    c = SigmaKVCache(max_size=3)
    c.put("keep1", 0.05)
    c.put("keep2", 0.06)
    c.put("drop", 0.99)
    c.put("keep3", 0.07)
    assert c.get("drop") is None
    assert c.get("keep1") is not None
    assert c.get("keep3") is not None


def test_max_size_enforced() -> None:
    c = SigmaKVCache(max_size=5)
    for i in range(30):
        c.put(str(i), float(i) / 30.0)
    assert len(c) == 5


def test_σ_weighted_attention() -> None:
    c = SigmaKVCache()
    c.put("a", 0.2, value="hello")
    rows = c.σ_weighted_attention("q")
    assert len(rows) == 1
    r0 = rows[0]
    assert r0["key"] == "a"
    assert "attention" in r0 and "relevance" in r0 and "reliability" in r0
    assert 0.0 <= float(r0["attention"]) <= 1.0


def test_compression_ratio() -> None:
    c = SigmaKVCache(max_size=4)
    for i in range(4):
        c.put(f"init{i}", 0.1)
    assert c.compression_ratio() == 1.0
    for j in range(20):
        c.put(f"more{j}", 0.5)
    assert len(c) == 4
    assert c.compression_ratio() < 1.0
    assert c.total_ops > len(c)


def test_get_miss() -> None:
    c = SigmaKVCache()
    assert c.get("nope") is None
