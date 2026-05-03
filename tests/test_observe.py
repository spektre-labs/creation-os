# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from cos.observe import SigmaObserve


def test_trace_request() -> None:
    o = SigmaObserve()
    sp = o.trace_request("a", "b", 0.3, "ACCEPT")
    assert sp["name"] == "cos.request"


def test_histogram() -> None:
    o = SigmaObserve()
    for s in (0.1, 0.4, 0.9):
        o.trace_request("p", "r", s, "ACCEPT")
    h = o.sigma_histogram()
    assert h["n"] == 3


def test_drift_alert() -> None:
    o = SigmaObserve()
    a = o.alert_on_drift(0.2, 0.9, 0.5)
    assert a["alert"] is True


def test_dashboard() -> None:
    o = SigmaObserve()
    o.trace_request("x", "y", 0.2, "ACCEPT")
    o.record_latency_ms(12.0)
    d = o.dashboard_data()
    assert d["n_requests"] == 1
    assert "verdict_distribution" in d


def test_layer_trace() -> None:
    o = SigmaObserve()
    o.per_layer_trace([{"layer": "L1", "sigma": 0.1, "info": "ok"}])
    assert len(o.layer_traces) == 1


def test_cost_tracking() -> None:
    o = SigmaObserve()
    o.cost_tracking(route="t", sigma=0.2, cheap=True)
    assert o.dashboard_data()["total_cost_units"] > 0
