# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for Engram v2 (σ-gated store, graph, staleness, consolidation)."""
from __future__ import annotations

import json
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.engram_graph import KnowledgeGraph  # noqa: E402
from cos.engram_v2 import EngramV2  # noqa: E402
from cos.sigma_gate_core import Verdict  # noqa: E402


def test_store_only_on_accept() -> None:
    eg = EngramV2()
    r = eg.store("hello", "cli", 0.05, Verdict.RETHINK)
    assert r["stored"] is False
    assert not eg.layers["episodic"].all()


def test_recall_filters_by_effective_sigma() -> None:
    eg = EngramV2()
    eg.store("Python is used for data pipelines", "cli", 0.05, Verdict.ACCEPT)
    hits = eg.recall("python data", tau=0.3, max_results=5)
    assert len(hits) >= 1
    assert all(float(h["effective_sigma"]) < 0.3 for h in hits)


def test_staleness_increases_with_wall_clock() -> None:
    t0 = datetime(2026, 1, 1, tzinfo=timezone.utc)
    t1 = datetime(2026, 3, 1, tzinfo=timezone.utc)
    state: dict[str, datetime] = {"t": t0}
    eg = EngramV2(now_fn=lambda: state["t"])
    eg.store("stable content for staleness", "cli", 0.05, Verdict.ACCEPT)
    m = eg.layers["episodic"].all()[0]
    s0 = eg.compute_staleness(m)
    state["t"] = t1
    s1 = eg.compute_staleness(m)
    assert s1 >= s0


def test_consolidation_moves_episodic_to_semantic() -> None:
    eg = EngramV2()
    for _ in range(3):
        eg.store("repeat me for consolidation beta gamma", "cli", 0.05, Verdict.ACCEPT)
    assert len(eg.layers["episodic"].all()) == 3
    out = eg.consolidate()
    assert out["consolidated_groups"] >= 1
    assert len(eg.layers["semantic"].all()) >= 1
    assert len(eg.layers["episodic"].all()) == 0


def test_gdpr_forget_targeted() -> None:
    eg = EngramV2()
    eg.store("note one", "user:alice", 0.05, Verdict.ACCEPT)
    eg.store("note two", "user:bob", 0.05, Verdict.ACCEPT)
    f = eg.forget(query="user:alice", gdpr=True)
    assert f["forgotten"] == 1
    assert len(eg.layers["episodic"].all()) == 1


def test_decay_forget_high_sigma() -> None:
    old = datetime(2020, 1, 1, tzinfo=timezone.utc)
    eg = EngramV2(now_fn=lambda: datetime(2026, 1, 1, tzinfo=timezone.utc))
    eg.layers["episodic"].add(
        {
            "id": "x1",
            "content": "stale",
            "source": "cli",
            "sigma": 0.85,
            "timestamp": old.isoformat(),
            "access_count": 0,
            "type": "episodic",
            "memory_kind": "episode",
        }
    )
    n = eg.forget(max_sigma=0.5)["forgotten"]
    assert n >= 1


def test_knowledge_graph_traverse_respects_tau() -> None:
    kg = KnowledgeGraph(backend="sqlite")
    kg.add_node("Python", sigma=0.1)
    kg.add_node("Data", sigma=0.1)
    kg.add_edge("Python", "Data", relation="used_for", sigma=0.1)
    out = kg.traverse(["Python"], max_hops=2, tau=0.5)
    names = [x["entity"] for x in out]
    assert "Python" in names
    assert "Data" in names


def test_save_load_json_roundtrip(tmp_path: Path) -> None:
    eg = EngramV2()
    eg.store("roundtrip content", "cli", 0.04, Verdict.ACCEPT)
    p = tmp_path / "eg.json"
    eg.save_json(p)
    eg2 = EngramV2.load_json(p)
    assert len(eg2.layers["episodic"].all()) == 1


def test_cos_memory_cli_store_recall(tmp_path: Path) -> None:
    env = {**__import__("os").environ, "PYTHONPATH": str(_REPO / "python")}
    st = tmp_path / "mem.json"
    r1 = subprocess.run(
        [
            sys.executable,
            "-m",
            "cos",
            "memory",
            "--state-file",
            str(st),
            "--store",
            "Python powers data pipelines",
            "--sigma",
            "0.05",
            "--verdict",
            "ACCEPT",
        ],
        cwd=str(_REPO),
        env=env,
        capture_output=True,
        text=True,
        check=False,
    )
    assert r1.returncode == 0, r1.stderr
    body = json.loads(r1.stdout.strip())
    assert body.get("stored") is True
    r2 = subprocess.run(
        [
            sys.executable,
            "-m",
            "cos",
            "memory",
            "--state-file",
            str(st),
            "--recall",
            "data pipelines",
            "--tau",
            "0.4",
        ],
        cwd=str(_REPO),
        env=env,
        capture_output=True,
        text=True,
        check=False,
    )
    assert r2.returncode == 0, r2.stderr
    out = json.loads(r2.stdout.strip())
    assert len(out.get("recall", [])) >= 1


def test_graph_conflict_detection_stub() -> None:
    kg = KnowledgeGraph(backend="sqlite")
    new_fact = {"subject": "Python", "polarity": "is not slow", "entities": ["Python"]}
    kg.add_edge("Python", "fast", relation="is fast", sigma=0.1)
    conflicts = kg.conflict_detection(new_fact)
    assert isinstance(conflicts, list)


def test_integration_rethink_then_recall_empty() -> None:
    eg = EngramV2()
    eg.store("fragile claim", "cli", 0.2, Verdict.RETHINK)
    assert eg.recall("fragile", tau=0.99, max_results=5) == []
