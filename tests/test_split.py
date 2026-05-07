# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from cos.split import FleetManager, SigmaSplit, SplitRouter


def test_route_edge_low_sigma() -> None:
    s = SigmaSplit(sigma_edge_threshold=0.5)
    r = s.route(0.2)
    assert r["placement"] == "edge"


def test_route_cloud_high_sigma() -> None:
    s = SigmaSplit(sigma_edge_threshold=0.5)
    r = s.route(0.9)
    assert r["placement"] == "cloud"


def test_bandwidth_raises_threshold() -> None:
    s = SigmaSplit(sigma_edge_threshold=0.3)
    a = s.route(0.35, bandwidth_mbps=5.0)
    b = s.route(0.35, bandwidth_mbps=100.0)
    assert a["sigma_threshold_used"] >= b["sigma_threshold_used"]


def test_partition() -> None:
    s = SigmaSplit()
    p = s.partition(10, 0.4)
    assert len(p["edge_layers"]) + len(p["cloud_layers"]) == 10


def test_edge_cloud_forward() -> None:
    s = SigmaSplit()
    e = s.edge_forward(1, lambda z: z + 1)
    c = s.cloud_forward(2, lambda z: z * 2)
    assert e["output"] == 2 and c["output"] == 4


def test_privacy_mask() -> None:
    s = SigmaSplit()
    m = s.privacy_mask(0.9)
    assert m["send_allowed"] is False


class _GateEasyLow:
    def score(self, prompt: str, response: str):
        if "difficulty" in str(prompt).lower():
            return 0.1, "ACCEPT"
        return 0.12, "ACCEPT"


class _GateHardHigh:
    def score(self, prompt: str, response: str):
        if "difficulty" in str(prompt).lower():
            return 0.95, "ABSTAIN"
        return 0.15, "ACCEPT"


class _GateEdgeAbstainFallback:
    def score(self, prompt: str, response: str):
        if "difficulty" in str(prompt).lower():
            return 0.1, "ACCEPT"
        if "EDGE_BAD" in str(response):
            return 0.95, "ABSTAIN"
        return 0.11, "ACCEPT"


def test_route_easy_to_edge() -> None:
    r = SplitRouter(gate=_GateEasyLow(), edge_threshold=0.3)
    out = r.route("hello world", edge_fn=lambda p: f"ok:{p[:3]}")
    assert out["source"] == "edge"
    assert out["sigma_pre"] < 0.3


def test_route_hard_to_cloud() -> None:
    r = SplitRouter(gate=_GateHardHigh(), edge_threshold=0.3)
    out = r.route("complex reasoning task here")
    assert out["source"] == "cloud"
    assert out["sigma_pre"] >= 0.3


def test_fallback_to_cloud() -> None:
    r = SplitRouter(gate=_GateEdgeAbstainFallback(), edge_threshold=0.3)
    out = r.route(
        "question?",
        edge_fn=lambda _p: "EDGE_BAD output",
        cloud_fn=lambda _p: "CLOUD safer answer text",
    )
    assert out["source"] == "cloud_fallback"
    assert "CLOUD" in out["response"]


def test_edge_ratio() -> None:
    r = SplitRouter(gate=_GateEasyLow(), edge_threshold=0.3)
    r.route("a")
    r.route("b")
    assert r.edge_ratio() == 1.0


def test_cost_savings() -> None:
    r = SplitRouter(gate=_GateEasyLow(), edge_threshold=0.3)
    for _ in range(3):
        r.route("x")
    body = r.cost_savings(cloud_cost_per_call=0.01)
    assert body["cloud_calls_avoided"] == 3
    assert body["estimated_savings"] == 0.03


class _FleetGateStable:
    def score(self, prompt: str, response: str):
        return 0.2, "ACCEPT"


def test_fleet_register() -> None:
    fm = FleetManager(gate=_FleetGateStable())
    fm.register("esp", "tiny", 0.5)
    fm.register("gpu1", "server", 1.0)
    assert len(fm.devices) == 2


def test_fleet_assign_cheapest() -> None:
    fm = FleetManager(gate=_FleetGateStable())
    fm.register("mac", "server", 1.0)
    fm.register("esp", "tiny", 0.45)
    out = fm.assign("run this job")
    assert out.get("device") == "esp"
    assert out["capability"] == "tiny"
