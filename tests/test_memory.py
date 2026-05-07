# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import time
from pathlib import Path

from cos.memory import Memory, SigmaMemory


def test_store_returns_sigma(tmp_path: Path) -> None:
    mem = SigmaMemory(persist_dir=tmp_path)
    out = mem.store("hello world", context="session", memory_type="episodic")
    assert "σ" in out or "sigma" in out
    assert out["type"] == "episodic"
    assert len(mem.episodic) == 1


def test_store_working_memory(tmp_path: Path) -> None:
    mem = SigmaMemory(persist_dir=tmp_path)
    mem.store("wip note", memory_type="working")
    assert len(mem.working) == 1
    assert mem.working[0].memory_type == "working"


def test_recall_by_query(tmp_path: Path) -> None:
    mem = SigmaMemory(persist_dir=tmp_path)
    mem.write("Paris is the capital of France", memory_type="semantic", sigma=0.05, force=True)
    results = mem.recall("capital France")
    assert len(results) > 0
    assert "Paris" in results[0]["content"]


def test_recall_filters_high_sigma(tmp_path: Path) -> None:
    mem = SigmaMemory(persist_dir=tmp_path)
    mem.write("noisy", memory_type="semantic", sigma=0.95, force=True)
    mem.write("clean fact about cats", memory_type="semantic", sigma=0.1, force=True)
    results = mem.recall("cats", max_σ=0.5)
    assert all(r["σ"] <= 0.5 for r in results)
    contents = " ".join(r["content"] for r in results)
    assert "cats" in contents


def test_consolidate_working_to_episodic(tmp_path: Path) -> None:
    mem = SigmaMemory(persist_dir=tmp_path)
    mem.store("a", memory_type="working")
    mem.store("b", memory_type="working")
    out = mem.consolidate()
    assert out["working_to_episodic"] == 2
    assert len(mem.working) == 0
    assert len(mem.episodic) >= 2


def test_consolidate_frequent_to_semantic(tmp_path: Path) -> None:
    mem = SigmaMemory(persist_dir=tmp_path)
    txt = "repeatable episodic trace XYZ same"
    for _ in range(3):
        mem.write(txt, memory_type="episodic", sigma=0.2, force=True)
    out = mem.consolidate()
    assert out["episodic_to_semantic"] >= 1
    assert any(txt[:50] in s.content for s in mem.semantic)


def test_decay_increases_sigma_for_old(tmp_path: Path) -> None:
    mem = SigmaMemory(persist_dir=tmp_path, decay_rate=0.05)
    m = Memory("old", 0.2, memory_type="episodic")
    m.created = time.time() - 40 * 86400
    m.access_count = 0
    mem.episodic.append(m)
    n = mem.decay(max_age_days=30.0)
    assert n >= 1
    assert m.σ > 0.2


def test_decay_slowed_by_access(tmp_path: Path) -> None:
    mem = SigmaMemory(persist_dir=tmp_path, decay_rate=0.1)
    cold = Memory("cold", 0.2, memory_type="episodic")
    cold.created = time.time() - 40 * 86400
    cold.access_count = 0
    hot = Memory("hot", 0.2, memory_type="episodic")
    hot.created = time.time() - 40 * 86400
    hot.access_count = 100
    mem.episodic.extend([cold, hot])
    sig_cold = cold.σ
    sig_hot = hot.σ
    mem.decay(max_age_days=30.0)
    assert cold.σ > sig_cold
    assert hot.σ - sig_hot < cold.σ - sig_cold


def test_forget_removes_high_sigma(tmp_path: Path) -> None:
    mem = SigmaMemory(persist_dir=tmp_path)
    mem.write("bad", memory_type="semantic", sigma=0.96, force=True)
    mem.write("good", memory_type="semantic", sigma=0.1, force=True)
    n = mem._forget_high_sigma(0.95)
    assert n >= 1
    assert all(m.σ < 0.95 for m in mem.semantic)


def test_persist_and_load(tmp_path: Path) -> None:
    m1 = SigmaMemory(persist_dir=tmp_path)
    m1.write("persist me", memory_type="episodic", sigma=0.1, force=True)
    m1._save()
    m2 = SigmaMemory(persist_dir=tmp_path)
    assert any("persist me" in e.content for e in m2.episodic)


def test_write_and_recall_legacy(tmp_path: Path) -> None:
    mem = SigmaMemory(persist_dir=tmp_path)
    mem.write("Paris is the capital of France", "semantic", sigma=0.05)
    results = mem.recall("capital France")
    assert len(results) > 0
    assert "Paris" in results[0]["content"]


def test_write_blocked_high_sigma(tmp_path: Path) -> None:
    mem = SigmaMemory(persist_dir=tmp_path, write_threshold=0.3)
    result = mem.write("Fake fact", "semantic", sigma=0.9)
    assert result["written"] is False
    assert result["reason"] == "sigma_too_high"


def test_memory_types_stats(tmp_path: Path) -> None:
    mem = SigmaMemory(persist_dir=tmp_path)
    mem.write("User clicked button", "episodic", sigma=0.1)
    mem.write("Earth orbits Sun", "semantic", sigma=0.05)
    mem.write("Sort list: use quicksort", "procedural", sigma=0.1)
    stats = mem.stats()
    assert stats["by_type"]["episodic"] == 1
    assert stats["by_type"]["semantic"] == 1
    assert stats["by_type"]["procedural"] == 1


def test_forget_by_type(tmp_path: Path) -> None:
    mem = SigmaMemory(persist_dir=tmp_path)
    mem.write("temp fact", "episodic", sigma=0.1)
    assert mem.stats()["total"] == 1
    mem.forget(memory_type="episodic")
    assert mem.stats()["total"] == 0


def test_refresh_on_read(tmp_path: Path) -> None:
    mem = SigmaMemory(persist_dir=tmp_path)
    mem.write("important fact", "semantic", sigma=0.05)
    results = mem.recall("important")
    assert results[0]["access_count"] == 1
    results = mem.recall("important")
    assert results[0]["access_count"] == 2


def test_conflict_detection(tmp_path: Path) -> None:
    mem = SigmaMemory(persist_dir=tmp_path)
    mem.write("Capital of France is Paris", "semantic", sigma=0.05)
    result = mem.write("Capital of France is London", "semantic", sigma=0.8)
    assert result["written"] is False or result["reason"] == "conflict_higher_sigma"


def test_stats_keys(tmp_path: Path) -> None:
    mem = SigmaMemory(persist_dir=tmp_path)
    mem.write("fact 1", "semantic", sigma=0.1)
    mem.write("fact 2", "semantic", sigma=0.2)
    stats = mem.stats()
    assert stats["total"] == 2
    assert "avg_sigma" in stats or "avg_σ" in stats


def test_prune_on_overflow(tmp_path: Path) -> None:
    mem = SigmaMemory(persist_dir=tmp_path, max_entries=3)
    for i in range(5):
        mem.write(f"fact {i}", "semantic", sigma=0.1, force=True)
    assert len(mem.entries) <= 3
