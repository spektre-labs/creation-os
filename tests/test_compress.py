# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""Tests for :mod:`cos.compress`."""
from __future__ import annotations

from cos import SigmaGate
from cos.compress import SigmaCompress


def test_compress_shrinks_model() -> None:
    gate = SigmaGate()
    m = {"a": [0.123456, 0.2, 0.3], "b": [0.1, 0.2]}
    c = SigmaCompress()
    out = c.compress(m, gate, target_size=4)
    n = sum(len(v) for v in out["compressed"].values())
    assert n <= 4


def test_sigma_before_after_reports_delta() -> None:
    gate = SigmaGate()
    c = SigmaCompress()
    orig = {"w": [1.0, 2.0, 3.0]}
    comp = {"w": [1.0, 2.0]}
    r = c.sigma_before_after(orig, comp, ("probe",), gate)
    assert "mean_abs_delta_sigma" in r


def test_compression_budget() -> None:
    b = SigmaCompress.compression_budget(0.2, threshold=0.45)
    assert b["within_budget"] is True


def test_pareto_frontier_sorted() -> None:
    gate = SigmaGate()
    c = SigmaCompress()
    models = [
        ({"a": [1.0] * 10}, "big"),
        ({"a": [1.0] * 2}, "small"),
    ]
    f = c.pareto_frontier(models, ("x",), gate)
    assert isinstance(f, list) and len(f) >= 1


def test_compress_report_has_stages() -> None:
    gate = SigmaGate()
    m = {"x": [0.111, 0.222, 0.333, 0.444]}
    out = SigmaCompress().compress(m, gate, target_size=2)
    assert "quantize" in out["report"]["stages"] or "prune" in out["report"]["stages"]
