# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for ``cos.snapshot`` (no network)."""
from __future__ import annotations

import os
import tempfile

from cos.snapshot import ConversationHistory, StateSnapshot


def test_checkpoint_and_rollback(pipeline, snapshot_mgr) -> None:
    result = snapshot_mgr.checkpoint("initial")
    assert result["checkpointed"]

    pipeline.score("a", "b")
    pipeline.score("c", "d")
    assert pipeline.stats.total == 2

    result = snapshot_mgr.rollback()
    assert result["rolled_back"]
    assert pipeline.stats.total == 0


def test_list_snapshots(snapshot_mgr) -> None:
    snapshot_mgr.checkpoint("first")
    snapshot_mgr.checkpoint("second")

    snaps = snapshot_mgr.list_snapshots()
    assert len(snaps) == 2
    assert snaps[0]["label"] == "first"
    assert snaps[1]["label"] == "second"


def test_diff(pipeline, snapshot_mgr) -> None:
    snapshot_mgr.checkpoint("before")
    pipeline.score("x", "y")
    snapshot_mgr.checkpoint("after")

    d = snapshot_mgr.diff()
    assert d.get("n_changed", 0) > 0


def test_conversation_undo(history) -> None:
    history.add("user", "Hello")
    history.add("assistant", "Hi!", sigma=0.05)

    assert len(history.turns) == 2

    result = history.undo()
    assert result["undone"] == 1
    assert len(history.turns) == 1
    assert history.turns[0]["role"] == "user"


def test_conversation_redo(history) -> None:
    history.add("user", "Hello")
    history.add("assistant", "Hi!")

    history.undo()
    assert len(history.turns) == 1

    history.redo()
    assert len(history.turns) == 2


def test_conversation_to_messages() -> None:
    h = ConversationHistory()
    h.add("user", "Hello")
    h.add("assistant", "Hi!")

    msgs = h.to_messages()
    assert len(msgs) == 2
    assert msgs[0]["role"] == "user"
    assert msgs[1]["role"] == "assistant"


def test_snapshot_save_load() -> None:
    snap = StateSnapshot({"gate": {"ema": 0.5}}, {"label": "test"})

    path = os.path.join(tempfile.mkdtemp(), "test.json")
    snap.save(path)

    loaded = StateSnapshot.load(path)
    assert loaded.state == snap.state
    assert loaded.checksum == snap.checksum


def test_sigma_snapshot_save_load_diff(tmp_path) -> None:
    from cos import SigmaGate
    from cos.snapshot import SigmaSnapshot

    g = SigmaGate()
    p = tmp_path / "sys.json"
    SigmaSnapshot.save(g, {"nodes": 1}, {"models": []}, p)
    blob = SigmaSnapshot.load(p)
    assert "state" in blob and blob["state"]["gate"]


def test_sigma_snapshot_restore_point(tmp_path) -> None:
    from cos import SigmaGate
    from cos.snapshot import SigmaSnapshot

    g = SigmaGate()
    g.threshold_accept = 0.11
    p = tmp_path / "r.json"
    SigmaSnapshot.save(g, None, None, p)
    blob = SigmaSnapshot.load(p)
    g2 = SigmaGate()
    SigmaSnapshot.restore_point(blob, g2)
    assert abs(float(g2.threshold_accept) - 0.11) < 1e-5


def test_sigma_snapshot_verify_integrity() -> None:
    from cos.snapshot import SigmaSnapshot

    r = SigmaSnapshot.verify_integrity({"state": {"gate": {"a": 1}, "x": [2, 3]}})
    assert r["ok"] and len(r["component_hashes"]) == 2


def test_sigma_snapshot_periodic_save(tmp_path) -> None:
    from cos import SigmaGate
    from cos.snapshot import SigmaSnapshot

    ss = SigmaSnapshot()
    g = SigmaGate()
    p = tmp_path / "q.json"
    first = ss.periodic_save(0.0, g, {}, {}, p)
    assert first["saved"] is True
    second = ss.periodic_save(60.0, g, {}, {}, p)
    assert second.get("saved") is False


def test_sigma_snapshot_diff() -> None:
    from cos.snapshot import SigmaSnapshot

    a = {"state": {"gate": {"x": 1}}}
    b = {"state": {"gate": {"x": 2}}}
    d = SigmaSnapshot.diff(a, b)
    assert d["n_changed"] >= 1

