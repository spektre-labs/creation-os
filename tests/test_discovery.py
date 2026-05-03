# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.discovery`."""
from __future__ import annotations

from cos.discovery import SigmaDiscovery
from cos.sigma_gate import SigmaGate


def test_register_and_search() -> None:
    d = SigmaDiscovery(SigmaGate())
    c = {"agent_id": "r1", "capabilities": ["search", "summarize"]}
    d.register(c)
    hits = d.search("summarize")
    assert len(hits) == 1 and hits[0]["agent_id"] == "r1"


def test_match_picks_agent() -> None:
    d = SigmaDiscovery(SigmaGate())
    a1 = {"agent_id": "a1", "capabilities": ["math"]}
    a2 = {"agent_id": "a2", "capabilities": ["math", "code"]}
    d.register(a1)
    d.register(a2)
    m = d.match({"required_capabilities": ["math"]}, [a1, a2])
    assert m["ok"] is True
    assert m["best"] and m["best"]["agent_id"] in ("a1", "a2")


def test_sigma_capability_confidence() -> None:
    d = SigmaDiscovery(SigmaGate())
    r = d.sigma_capability_confidence({"capabilities": ["x", "y", "z"]})
    assert "confidence" in r and "sigma" in r


def test_stale_detection() -> None:
    d = SigmaDiscovery(SigmaGate())
    d.register({"agent_id": "stale_test", "capabilities": []})
    s = d.stale_detection("stale_test", max_age_s=-1.0)
    assert s["stale"] is True


def test_federation_merges() -> None:
    d = SigmaDiscovery()
    m = d.federation([{"a": {"k": 1}}, {"b": {"k": 2}}])
    assert m["n"] == 2 and m["agents"]["a"]["k"] == 1
