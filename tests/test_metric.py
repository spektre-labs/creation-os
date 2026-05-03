# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.metric`."""
from __future__ import annotations

import json

from cos.metric import SigmaMetrics


def test_counter_and_gauge() -> None:
    m = SigmaMetrics()
    assert m.counter("hits") == 1.0
    m.gauge("drift_score", 0.05)
    assert m.standard_metrics()["drift_score"] == 0.05


def test_histogram_and_verdict_rates() -> None:
    m = SigmaMetrics()
    m.histogram("latency", 12.0)
    m.observe_verdict("ACCEPT")
    m.observe_verdict("RETHINK")
    std = m.standard_metrics()
    assert std["accept_rate"] > 0.0


def test_record_sigma_for_percentiles() -> None:
    m = SigmaMetrics()
    for v in (0.1, 0.2, 0.3, 0.8, 0.9):
        m.record("sigma", v)
    std = m.standard_metrics()
    assert std["sigma_p95"] >= std["sigma_p50"]


def test_export_json_roundtrip() -> None:
    m = SigmaMetrics()
    m.counter("c", 2)
    j = m.export_json()
    assert "counters" in json.loads(j)


def test_export_prometheus_text() -> None:
    m = SigmaMetrics()
    m.gauge("x", 1.0)
    txt = m.export_prometheus()
    assert "cos_gauge" in txt


def test_dashboard_and_otlp_shape() -> None:
    m = SigmaMetrics()
    d = m.dashboard_summary()
    assert "standard_metrics" in d
    o = m.export_opentelemetry()
    assert "resourceLogs" in o
