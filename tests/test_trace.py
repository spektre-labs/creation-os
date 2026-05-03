# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""Tests for :mod:`cos.trace`."""
from __future__ import annotations

import json

from cos.trace import SigmaTrace


def test_trace_has_uuid() -> None:
    tr = SigmaTrace()
    assert len(tr.trace_id) == 36


def test_trace_span_sets_duration() -> None:
    tr = SigmaTrace()
    with tr.span("generate") as rec:
        rec["sigma"] = 0.1
        rec["verdict"] = "ACCEPT"
    assert tr.spans[0]["duration_ms"] is not None
    assert tr.spans[0]["sigma"] == 0.1


def test_trace_export_json() -> None:
    tr = SigmaTrace("fixed-id")
    with tr.span("sigma_gate"):
        pass
    jtr = json.loads(tr.export_json())
    assert jtr["trace_id"] == "fixed-id"
    assert len(jtr["spans"]) == 1


def test_trace_export_otel_shape() -> None:
    tr = SigmaTrace()
    with tr.span("tokenize") as r:
        r["sigma"] = 0.2
    ot = tr.export_opentelemetry()
    assert "resourceSpans" in ot
    assert ot["resourceSpans"][0]["scopeSpans"][0]["spans"]


def test_trace_waterfall_non_empty() -> None:
    tr = SigmaTrace()
    with tr.span("input_guard"):
        pass
    w = tr.waterfall_view()
    assert "input_guard" in w


def test_trace_bottleneck() -> None:
    tr = SigmaTrace()
    with tr.span("slow") as a:
        a["sigma"] = 0.9
    tr.spans[-1]["duration_ms"] = 50.0
    with tr.span("fast") as b:
        b["sigma"] = 0.1
    tr.spans[-1]["duration_ms"] = 1.0
    bn = tr.bottleneck_detection()
    assert bn["slowest"]["name"] == "slow"
    assert bn["highest_sigma"]["name"] == "slow"
