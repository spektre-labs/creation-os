# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
from __future__ import annotations

from cos.drift import SigmaDrift


def test_set_baseline() -> None:
    d = SigmaDrift()
    d.set_baseline([0.1, 0.2, 0.3])
    assert d.baseline is not None
    assert d.baseline["n"] == 3


def test_no_drift_same_distribution() -> None:
    d = SigmaDrift()
    xs = [0.2, 0.22, 0.21, 0.19, 0.2]
    d.set_baseline(xs)
    r = d.detect(list(xs))
    assert r["alert"] is False
    assert r["drift"] < 0.01


def test_drift_detected_shifted_mean() -> None:
    d = SigmaDrift()
    d.set_baseline([0.1] * 20)
    r = d.detect([0.85] * 20)
    assert r["alert"] is True
    assert r["reason"] == "σ distribution shifted"


def test_drift_score_normalized() -> None:
    d = SigmaDrift()
    d.set_baseline([0.5] * 30)
    r = d.detect([0.5] * 30)
    assert 0.0 <= r["drift"] <= 1.0


def test_no_baseline_returns_zero() -> None:
    d = SigmaDrift()
    r = d.detect([0.1, 0.2])
    assert r["drift"] == 0.0
    assert r["alert"] is False
    assert r["reason"] == "no baseline"


def test_kl_divergence_calculated() -> None:
    d = SigmaDrift()
    d.set_baseline([0.2, 0.25, 0.22, 0.21])
    r = d.detect([0.2, 0.24, 0.23, 0.22])
    assert "kl_divergence" in r
    assert r["kl_divergence"] >= 0.0
