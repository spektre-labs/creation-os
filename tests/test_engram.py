# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import json
from pathlib import Path

from cos.engram import Engram


def test_begin_session_increments(tmp_path: Path) -> None:
    p = tmp_path / "e.json"
    e = Engram(path=p)
    assert e.identity["sessions"] == 0
    n1 = e.begin_session()
    n2 = e.begin_session()
    assert n1 == 1
    assert n2 == 2
    assert e.identity["sessions"] == 2


def test_record_event_with_sigma(tmp_path: Path) -> None:
    e = Engram(path=tmp_path / "e.json")
    e.record_event("unit", "hello", σ=0.12)
    assert len(e.narrative) == 1
    assert e.narrative[0]["σ"] == 0.12
    assert e.narrative[0]["type"] == "unit"


def test_identity_sigma_calculated(tmp_path: Path) -> None:
    e = Engram(path=tmp_path / "e.json")
    e.begin_session()
    e.record_event("note", "second", σ=0.2)
    s = e.identity_σ()
    assert isinstance(s, float)
    assert 0.0 <= s <= 1.0


def test_continuity_check_5_axes(tmp_path: Path) -> None:
    e = Engram(path=tmp_path / "e.json")
    e.record_event("ev", "one", σ=0.1)
    out = e.continuity_check()
    assert set(out["axes"].keys()) == {
        "memory",
        "goals",
        "self_correction",
        "style_consistency",
        "identity_stable",
    }
    assert 0.0 <= float(out["continuity_score"]) <= 1.0


def test_persist_and_load(tmp_path: Path) -> None:
    p = tmp_path / "persist.json"
    a = Engram(path=p)
    a.begin_session()
    a.record_event("x", "y", σ=0.05)
    b = Engram(path=p)
    assert b.identity["sessions"] == a.identity["sessions"]
    assert any(e.get("type") == "x" for e in b.narrative)


def test_self_model_update(tmp_path: Path) -> None:
    e = Engram(path=tmp_path / "e.json")
    e.update_self_model("current_goal", "test goal", σ=0.11)
    assert "current_goal" in e.self_model
    assert e.self_model["current_goal"]["σ"] == 0.11


def test_narrative_max_1000(tmp_path: Path) -> None:
    e = Engram(path=tmp_path / "e.json")
    for i in range(1001):
        e.record_event("fill", str(i), σ=0.01)
    assert len(e.narrative) == 1000
    raw = json.loads(Path(tmp_path / "e.json").read_text(encoding="utf-8"))
    assert len(raw["narrative"]) == 1000


def test_end_session_records(tmp_path: Path) -> None:
    e = Engram(path=tmp_path / "e.json")
    e.begin_session()
    e.end_session("done")
    types = [row["type"] for row in e.narrative]
    assert "session_end" in types


def test_identity_invariant_empty(tmp_path: Path) -> None:
    e = Engram(path=tmp_path / "e.json")
    inv = e.identity_invariant()
    assert inv["identity_preserved"] is False


def test_identity_invariant_preserved_stable_low_sigma(tmp_path: Path) -> None:
    e = Engram(path=tmp_path / "e.json")
    for i in range(12):
        e.record_event("e", str(i), σ=0.1)
    inv = e.identity_invariant()
    assert inv["sigma_avg"] < 0.5
    assert inv["drift"] < 0.3
    assert inv["identity_preserved"] is True
    assert inv["invariant"] == "σ"


def test_identity_invariant_fails_high_drift(tmp_path: Path) -> None:
    e = Engram(path=tmp_path / "e.json")
    sigmas = [0.1] * 9 + [0.95]
    for i, s in enumerate(sigmas):
        e.record_event("e", str(i), σ=s)
    inv = e.identity_invariant()
    assert inv["identity_preserved"] is False
    assert inv["drift"] >= 0.3


def test_identity_invariant_fails_high_mean(tmp_path: Path) -> None:
    e = Engram(path=tmp_path / "e.json")
    for i in range(12):
        e.record_event("e", str(i), σ=0.55)
    inv = e.identity_invariant()
    assert inv["identity_preserved"] is False
