# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.drift`."""
from __future__ import annotations

from cos.drift import SigmaDrift
from cos.sigma_gate import SigmaGate


def test_baseline_and_detect() -> None:
    g = SigmaGate()
    d = SigmaDrift()
    d.baseline(g, [{"prompt": "a", "response": "1"}, {"prompt": "b", "response": "2"}])
    r = d.detect(g, [{"prompt": "a", "response": "1"}, {"prompt": "b", "response": "2"}])
    assert r["drift_score"] >= 0.0


def test_detect_without_baseline() -> None:
    d = SigmaDrift()
    r = d.detect(SigmaGate(), [{"prompt": "x", "response": "y"}])
    assert r.get("error") == "no_baseline"


def test_alert_threshold() -> None:
    a = SigmaDrift.alert_threshold({"drift_score": 0.9}, limit=0.2)
    assert a["alert"] is True


def test_auto_recalibrate_returns_taus() -> None:
    g = SigmaGate()
    d = SigmaDrift()
    out = d.auto_recalibrate(g, [{"prompt": str(i), "response": str(i * 2)} for i in range(5)])
    assert "threshold_accept" in out and "threshold_abstain" in out


def test_root_cause() -> None:
    r = SigmaDrift.root_cause({"kl_sym": 0.5, "w1_proxy": 0.01})
    assert "distribution_shape_shift" in r["likely_causes"]


def test_temporal_trend() -> None:
    g = SigmaGate()
    d = SigmaDrift()
    d.baseline(g, [{"prompt": "p", "response": "r"}])
    d.detect(g, [{"prompt": "p", "response": "r"}])
    tr = d.temporal_trend(7.0)
    assert tr["n"] >= 1
