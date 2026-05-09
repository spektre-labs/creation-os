# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.edge` (tier heuristics and :class:`~cos.edge.EdgeSigmaGate`)."""
from __future__ import annotations

from cos.edge import EdgeProfile, EdgeSigmaGate, _tier_config


def test_detect_returns_tier(monkeypatch) -> None:
    monkeypatch.setattr("cos.edge._get_ram_mb", lambda: 128)
    assert EdgeProfile.detect()["tier"] == 1
    monkeypatch.setattr("cos.edge._get_ram_mb", lambda: 512)
    assert EdgeProfile.detect()["tier"] == 2
    monkeypatch.setattr("cos.edge._get_ram_mb", lambda: 8192)
    assert EdgeProfile.detect()["tier"] == 3


def test_tier_config_valid() -> None:
    for t in (1, 2, 3):
        cfg = _tier_config(t)
        assert "model" in cfg and "probes" in cfg and "note" in cfg
        assert isinstance(cfg["probes"], list)
        assert len(cfg["probes"]) >= 1


def test_edge_gate_scores() -> None:
    eg = EdgeSigmaGate(tier=1)
    s, v = eg.score("What is 2+2?", "4")
    assert 0.0 <= s <= 1.0
    assert v in ("ACCEPT", "RETHINK", "ABSTAIN")


def test_can_run_model(monkeypatch) -> None:
    monkeypatch.setattr("cos.edge._get_ram_mb", lambda: 4096)
    r = EdgeSigmaGate(tier=2).can_run(0.5)
    assert isinstance(r["can_run"], bool)
    assert r["ram_mb"] >= 1
    assert r["needed_mb"] > 0
    assert r["model_size_b"] == 0.5
