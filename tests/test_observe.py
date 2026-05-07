# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
from __future__ import annotations

import json
from pathlib import Path

from cos.observe import SigmaObserve


def test_record_with_trace(tmp_path: Path) -> None:
    o = SigmaObserve(log_dir=str(tmp_path))
    tr = {"levels": {"L1": 0.2}, "thresholds": {"abstain": 0.85}, "trigger": "entropy"}
    e = o.record_with_trace("p", "r", 0.4, "ACCEPT", 5.0, trace=tr)
    assert e["sigma"] == 0.4
    assert e["trace"] == tr
    assert o.window[0]["trace"] == tr


def test_record_stores_entry(tmp_path: Path) -> None:
    o = SigmaObserve(log_dir=str(tmp_path))
    e = o.record("p", "r", 0.25, "ACCEPT", 12.3)
    assert e["sigma"] == 0.25
    assert e["verdict"] == "ACCEPT"
    assert e["prompt_len"] == 1
    assert len(o.window) == 1


def test_summary_calculates_stats(tmp_path: Path) -> None:
    o = SigmaObserve(log_dir=str(tmp_path))
    for s, v in ((0.2, "ACCEPT"), (0.2, "ACCEPT"), (0.8, "RETHINK")):
        o.record("a", "b", s, v, 10.0)
    s = o.summary()
    assert s["count"] == 3
    assert s["σ_avg"] == round((0.2 + 0.2 + 0.8) / 3, 4)
    assert s["accept_rate"] == round(2 / 3, 3)
    assert s["rethink_rate"] == round(1 / 3, 3)


def test_alert_on_high_sigma(tmp_path: Path) -> None:
    o = SigmaObserve(log_dir=str(tmp_path))
    for _ in range(10):
        o.record("a", "b", 0.55, "ACCEPT", 1.0)
    assert any(a.get("type") == "high_σ_avg" for a in o.alerts)


def test_alert_consecutive_abstain(tmp_path: Path) -> None:
    o = SigmaObserve(log_dir=str(tmp_path))
    for _ in range(10):
        o.record("a", "b", 0.2, "ACCEPT", 1.0)
    o.record("a", "b", 0.9, "ABSTAIN", 1.0)
    o.record("a", "b", 0.9, "ABSTAIN", 1.0)
    assert any(a.get("type") == "consecutive_abstain" for a in o.alerts)


def test_log_to_disk_creates_file(tmp_path: Path) -> None:
    o = SigmaObserve(log_dir=str(tmp_path))
    o.record("hi", "yo", 0.1, "ACCEPT", 2.0)
    log_files = list(tmp_path.glob("sigma_*.jsonl"))
    assert len(log_files) == 1
    line = log_files[0].read_text(encoding="utf-8").strip()
    assert json.loads(line)["verdict"] == "ACCEPT"


def test_empty_summary(tmp_path: Path) -> None:
    o = SigmaObserve(log_dir=str(tmp_path))
    assert o.summary() == {"count": 0}


def test_window_size_limits(tmp_path: Path) -> None:
    o = SigmaObserve(log_dir=str(tmp_path), window_size=3)
    for i in range(5):
        o.record("p", "r", 0.1 * i, "ACCEPT", 1.0)
    assert len(o.window) == 3
    s = o.summary()
    assert s["count"] == 3
