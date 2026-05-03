# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from cos.split import SigmaSplit


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
