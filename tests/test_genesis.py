# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from pathlib import Path

from cos.eval.genesis import GenesisCheck


def test_genesis_note_not_agi(tmp_path: Path) -> None:
    g = GenesisCheck(engram_path=tmp_path / "e.json")
    r = g.run()
    assert "NOT AGI ACHIEVED" in r["note"]
    assert r["total"] == 6
    assert len(r["stages"]) == 6


def test_genesis_stage_keys(tmp_path: Path) -> None:
    g = GenesisCheck(engram_path=tmp_path / "e2.json")
    r = g.run()
    for key in ("boot", "perceive", "reason", "act", "learn", "persist"):
        assert key in r["stages"]
        assert "passed" in r["stages"][key]


def test_genesis_all_pass_with_isolated_engram(tmp_path: Path) -> None:
    g = GenesisCheck(engram_path=tmp_path / "e3.json")
    r = g.run()
    assert r["stages"]["persist"].get("passed") is True
    assert r["stages"]["boot"].get("passed") is True
