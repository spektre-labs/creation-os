# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.marketplace`."""
from __future__ import annotations

from cos.marketplace import SigmaMarketplace


def test_publish_and_download_probe() -> None:
    m = SigmaMarketplace()
    o = m.publish_probe({"name": "x"}, {"task": "qa"}, {"sigma": 0.1})
    d = m.download_probe(o["probe_id"])
    assert d["ok"] is True and d["sha256"] == o["sha256"]


def test_search_probes() -> None:
    m = SigmaMarketplace()
    m.publish_probe({"id": 1}, {"task": "summarize"}, {})
    hits = m.search_probes("summarize", "mini")
    assert len(hits) >= 1


def test_publish_model_profile_leaderboard() -> None:
    m = SigmaMarketplace()
    m.publish_model_profile({"id": "m1"}, {"sigma": 0.4, "name": "Truth"})
    m.publish_model_profile({"id": "m2"}, {"sigma": 0.1, "name": "Truth"})
    lb = m.leaderboard()
    assert lb[0]["sigma"] <= lb[-1]["sigma"]


def test_community_benchmarks() -> None:
    m = SigmaMarketplace()
    r = m.add_community_benchmark("custom", [{}])
    assert r["registered"] is True


def test_sigma_score_as_badge() -> None:
    b = SigmaMarketplace.sigma_score_as_badge(0.1)
    assert "Spektre" in b
