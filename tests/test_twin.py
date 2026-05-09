# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.genesis.twin` (cognitive twin lab)."""
from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.genesis.twin import CognitiveTwin  # noqa: E402


class _FixedSigma:
    def __init__(self, sigma: float) -> None:
        self._σ = float(sigma)

    def score(self, _p: str, _r: str):
        return self._σ, "ACCEPT"


class _DriftOnMismatch:
    def score(self, prompt: str, response: str):
        if "twin predicts k=wrong" in prompt or "twin predicts k=None" in prompt:
            return 0.75, "RETHINK"
        return 0.05, "ACCEPT"


class _WhatIfGate:
    def score(self, prompt: str, response: str):
        if "reality shows k=smooth" in response:
            return 0.1, "ACCEPT"
        if "reality shows k=rough" in response:
            return 0.85, "RETHINK"
        return 0.5, "ACCEPT"


class _TrendGate:
    """Increasing sigma per :meth:`score` call."""

    def __init__(self, start: float = 0.38, step: float = 0.09) -> None:
        self._n = 0
        self._start = start
        self._step = step

    def score(self, _p: str, _r: str):
        self._n += 1
        s = min(0.95, self._start + (self._n - 1) * self._step)
        return s, "ACCEPT"


def test_observe_and_predict() -> None:
    t = CognitiveTwin(gate=_FixedSigma(0.1))
    t.observe_real("load", 42)
    assert t.predict_twin("load") is None
    t.sync()
    assert t.predict_twin("load") == 42


def test_sync_updates_drifted() -> None:
    t = CognitiveTwin(gate=_DriftOnMismatch())
    t.observe_real("k", "truth")
    t.twin_state["k"] = {"value": "wrong", "timestamp": 0}
    out = t.sync()
    assert out["updates"] >= 1
    assert t.predict_twin("k") == "truth"
    assert out["drifts"]


def test_sync_fidelity_measurement() -> None:
    t = CognitiveTwin(gate=_FixedSigma(0.2))
    t.observe_real("x", 1)
    out = t.sync()
    assert out["fidelity"] == round(1.0 - 0.2, 4)
    assert out["avg_σ"] == 0.2


def test_what_if_analysis() -> None:
    t = CognitiveTwin(gate=_WhatIfGate())
    t.observe_real("k", "baseline")
    t.twin_state["k"] = {"value": "model", "timestamp": 0}
    w = t.what_if("k", "smooth")
    assert w["σ_hypothetical"] < w["σ_current"]
    assert w["recommendation"] == "change is beneficial"


def test_predictive_alert_detects_drift() -> None:
    t = CognitiveTwin(gate=_TrendGate(start=0.46, step=0.08))
    for _ in range(5):
        t.observe_real("metric", 1)
        t.sync()
    alert = t.predictive_alert(threshold=0.6)
    assert alert["alert"] is True
    assert alert["severity"] in ("HIGH", "MEDIUM")


def test_fidelity_report() -> None:
    t = CognitiveTwin(name="lab-twin", gate=_FixedSigma(0.15))
    t.observe_real("a", 1)
    t.sync()
    rep = t.fidelity_report()
    assert rep["name"] == "lab-twin"
    assert rep["syncs"] == 1
    assert rep["real_keys"] == 1
    assert "avg_fidelity" in rep
