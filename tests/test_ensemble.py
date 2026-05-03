# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for ``cos.ensemble``."""
from __future__ import annotations

from cos.ensemble import SigmaEnsemble


def test_ensemble_score_weighted_mean() -> None:
    e = SigmaEnsemble()
    vals = {k: 0.4 for k in e.probe_names}
    vals["entropy"] = 0.8
    out = e.score(vals)
    assert "sigma" in out and "verdict" in out
    assert 0.35 <= out["sigma"] <= 0.55


def test_ensemble_adaptive_weights_inverse_error() -> None:
    e = SigmaEnsemble(probes=("a", "b"))
    rows = [
        {"target_sigma": 0.5, "a": 0.9, "b": 0.5},
        {"target_sigma": 0.5, "a": 0.85, "b": 0.52},
    ]
    w = e.adaptive_weights(rows)
    assert w["b"] > w["a"]


def test_ensemble_disagreement_triggers_rethink() -> None:
    e = SigmaEnsemble(probes=("x", "y", "z"))
    out = e.score({"x": 0.1, "y": 0.95, "z": 0.9})
    assert out["disagreement"] > 0.28
    assert out["verdict"] == "RETHINK"


def test_ensemble_leave_one_out_spread() -> None:
    e = SigmaEnsemble(probes=("p", "q", "r"))
    pv = {"p": 0.4, "q": 0.45, "r": 0.5}
    lo = e.leave_one_out(pv)
    assert "spread" in lo and "per_drop" in lo
    assert lo["spread"] >= 0.0


def test_ensemble_fallback_median_on_partial_none() -> None:
    e = SigmaEnsemble(probes=("a", "b"))
    out = e.fallback({"a": 0.2, "b": None})
    assert out["mode"] == "median_fallback"
    assert abs(out["sigma"] - 0.2) < 1e-6


def test_ensemble_custom_probe_list() -> None:
    e = SigmaEnsemble(probes=("m1", "m2"), weights={"m1": 3.0, "m2": 1.0})
    out = e.score({"m1": 0.0, "m2": 1.0})
    assert out["sigma"] < 0.3


def test_ensemble_empty_probe_values_neutral() -> None:
    e = SigmaEnsemble(probes=("u",))
    out = e.score({})
    assert out["sigma"] == 0.5
