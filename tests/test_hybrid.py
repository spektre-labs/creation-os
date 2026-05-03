# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.hybrid`."""
from __future__ import annotations

from cos.hybrid import SigmaHybrid
from cos.sigma_gate import SigmaGate


def test_route_accepts_edge() -> None:
    gate = SigmaHybrid(gate=SigmaGate()).gate

    class E:
        def generate(self, p: str) -> str:
            return "4"

    class C:
        def generate(self, p: str) -> str:
            return "cloud"

    h = SigmaHybrid(gate=gate)
    r = h.route("What is 2+2?", E(), C())
    assert r["source"] == "edge"


def test_route_abstain_no_cloud() -> None:
    gate = SigmaGate()

    class E:
        def generate(self, p: str) -> str:
            return ""

    class C:
        def generate(self, p: str) -> str:
            return "should-not-run"

    h = SigmaHybrid(gate=gate)
    r = h.route("qa", E(), C())
    assert r["source"] == "abstain"
    assert "should-not-run" not in str(r.get("cloud_response", ""))


def test_route_rethink_cloud() -> None:
    gate = SigmaGate()

    class E:
        def generate(self, p: str) -> str:
            return "x" * 30

    class C:
        def generate(self, p: str) -> str:
            return "4"

    h = SigmaHybrid(gate=gate)
    r = h.route("What is 2+2?", E(), C())
    assert r["source"] == "cloud"


def test_adaptive_threshold_network() -> None:
    t = SigmaHybrid.adaptive_threshold(80.0, 90.0, 0.2)
    assert t["tau_accept"] >= 0.3


def test_prefetch() -> None:
    h = SigmaHybrid()

    class E:
        def generate(self, p: str) -> str:
            return "cached"

    r = h.prefetch(["q1"], E())
    assert r["prefetched"] == 1
    out = h.route("q1", E(), E())
    assert out["edge_response"] == "cached"


def test_privacy_routing_pii() -> None:
    r = SigmaHybrid.privacy_routing("x", True)
    assert r["allow_cloud"] is False


def test_cost_comparison() -> None:
    c = SigmaHybrid.cost_comparison(0.01, 0.05)
    assert c["savings_when_edge_chosen"] >= 0.0
