# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.offline_eval`."""
from __future__ import annotations

import json

from cos.offline_eval import SigmaOfflineEval


def test_batch_size_auto() -> None:
    assert SigmaOfflineEval.batch_size_auto(8.0) >= 8


def test_progress_bar() -> None:
    s = SigmaOfflineEval.progress_bar(10, 5)
    assert "5/10" in s


def test_checkpoint_every() -> None:
    assert SigmaOfflineEval.checkpoint_every(25) == 25


def test_run_offline_incremental(tmp_path) -> None:
    ev = SigmaOfflineEval()
    ev._checkpoint_interval = 1
    ds = tmp_path / "d.jsonl"
    out = tmp_path / "out.jsonl"
    ck = tmp_path / "c.json"
    ds.write_text(
        '{"prompt":"a","response":"b"}\n{"prompt":"c","response":"d"}\n',
        encoding="utf-8",
    )
    r = ev.run_offline(tmp_path / "model.txt", ds, out, checkpoint_path=ck)
    assert r["ok"] is True
    assert out.is_file()
    assert ck.is_file()


def test_resume_roundtrip(tmp_path) -> None:
    ck = tmp_path / "x.json"
    ck.write_text(json.dumps({"next_index": 3, "total": 10}), encoding="utf-8")
    d = SigmaOfflineEval.resume(ck)
    assert d["ok"] and d["next_index"] == 3
