# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from cos.memory import SigmaMemory


def test_write_and_recall() -> None:
    mem = SigmaMemory()
    mem.write("Paris is the capital of France", "semantic", sigma=0.05)
    results = mem.recall("capital France")
    assert len(results) > 0
    assert "Paris" in results[0]["content"]


def test_write_blocked_high_sigma() -> None:
    mem = SigmaMemory(write_threshold=0.3)
    result = mem.write("Fake fact", "semantic", sigma=0.9)
    assert result["written"] is False
    assert result["reason"] == "sigma_too_high"


def test_memory_types() -> None:
    mem = SigmaMemory()
    mem.write("User clicked button", "episodic", sigma=0.1)
    mem.write("Earth orbits Sun", "semantic", sigma=0.05)
    mem.write("Sort list: use quicksort", "procedural", sigma=0.1)
    stats = mem.stats()
    assert stats["by_type"]["episodic"] == 1
    assert stats["by_type"]["semantic"] == 1
    assert stats["by_type"]["procedural"] == 1


def test_forget() -> None:
    mem = SigmaMemory()
    mem.write("temp fact", "episodic", sigma=0.1)
    assert mem.stats()["total"] == 1
    mem.forget(memory_type="episodic")
    assert mem.stats()["total"] == 0


def test_refresh_on_read() -> None:
    mem = SigmaMemory()
    mem.write("important fact", "semantic", sigma=0.05)
    results = mem.recall("important")
    assert results[0]["access_count"] == 1
    results = mem.recall("important")
    assert results[0]["access_count"] == 2


def test_conflict_detection() -> None:
    mem = SigmaMemory()
    mem.write("Capital of France is Paris", "semantic", sigma=0.05)
    result = mem.write("Capital of France is London", "semantic", sigma=0.8)
    assert result["written"] is False or result["reason"] == "conflict_higher_sigma"


def test_stats() -> None:
    mem = SigmaMemory()
    mem.write("fact 1", "semantic", sigma=0.1)
    mem.write("fact 2", "semantic", sigma=0.2)
    stats = mem.stats()
    assert stats["total"] == 2
    assert "avg_sigma" in stats


def test_prune_on_overflow() -> None:
    mem = SigmaMemory(max_entries=3)
    for i in range(5):
        mem.write(f"fact {i}", "semantic", sigma=0.1, force=True)
    assert len(mem.entries) <= 3
