# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import pytest

from cos.sigma_gate import SigmaGate
from cos.interp.steer import SigmaSteer


def test_should_steer_high_sigma() -> None:
    st = SigmaSteer()
    r = st.should_steer("q", "qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq")
    assert r["steer"] is True
    assert r["σ"] > 0.7
    assert r["urgency"] == "high"


def test_should_not_steer_low_sigma() -> None:
    gate = SigmaGate()
    gate.threshold_accept = 0.26
    st = SigmaSteer(gate=gate)
    r = st.should_steer("What is 2+2?", "4")
    assert r["steer"] is False
    assert r["verdict"] == "ACCEPT"


def test_identify_target_hallucination(monkeypatch: pytest.MonkeyPatch) -> None:
    pytest.importorskip("numpy")
    from cos.interp.sae import SigmaSAE

    gate = SigmaGate()
    sae = SigmaSAE(gate=gate, input_dim=16, hidden_dim=64)
    st = SigmaSteer(gate=gate, sae=sae)

    def fake_analyze(_cases):  # noqa: ARG001
        return {
            "hallucination_features": [{"feature_id": 7, "sigma_association": 0.82}],
            "coherence_features": [],
        }

    monkeypatch.setattr(sae, "analyze_σ_drivers", fake_analyze)
    t = st.identify_target("p", "r")
    assert t["target"] == 7
    assert t["action"] == "suppress"
    assert t["method"] == "sae"
    assert t["σ_association"] == pytest.approx(0.82)


def test_steer_full_pipeline(monkeypatch: pytest.MonkeyPatch) -> None:
    pytest.importorskip("numpy")
    from cos.interp.sae import SigmaSAE

    gate = SigmaGate()
    sae = SigmaSAE(gate=gate, input_dim=8, hidden_dim=32)
    st = SigmaSteer(gate=gate, sae=sae)
    monkeypatch.setattr(gate, "score", lambda _p, _r, **_k: (0.88, "ABSTAIN"))

    def fake_analyze(_cases):  # noqa: ARG001
        return {
            "hallucination_features": [{"feature_id": 3, "sigma_association": 0.9}],
            "coherence_features": [],
        }

    monkeypatch.setattr(sae, "analyze_σ_drivers", fake_analyze)
    act = [0.05] * 8
    out = st.steer(act, "prompt text", "dubious answer")
    assert out["steered"] is True
    assert out["feature_id"] == 3
    assert out["action"] == "suppress"
    assert len(out["activation"]) == 8


def test_trajectory_correct_on_track() -> None:
    st = SigmaSteer()
    r = st.trajectory_correct([0.1, 0.12, 0.11], [0.1, 0.15, 0.12], tolerance=0.2)
    assert r["needs_correction"] is False
    assert r["action"] == "on track"


def test_trajectory_correct_diverged() -> None:
    st = SigmaSteer()
    r = st.trajectory_correct([0.2, 0.9, 0.3], [0.2, 0.25, 0.28], tolerance=0.2)
    assert r["needs_correction"] is True
    assert r["divergence_point"] == 1
    assert "steer" in r["action"]


def test_report_counts(monkeypatch: pytest.MonkeyPatch) -> None:
    pytest.importorskip("numpy")
    from cos.interp.sae import SigmaSAE

    gate = SigmaGate()
    sae = SigmaSAE(gate=gate, input_dim=6, hidden_dim=24)
    st = SigmaSteer(gate=gate, sae=sae)

    calls: dict[str, int] = {"n": 0}

    def fake_analyze(_cases):  # noqa: ARG001
        calls["n"] += 1
        if calls["n"] == 1:
            return {
                "hallucination_features": [{"feature_id": 1, "sigma_association": 0.8}],
                "coherence_features": [],
            }
        return {
            "hallucination_features": [],
            "coherence_features": [{"feature_id": 2, "sigma_association": 0.1}],
        }

    monkeypatch.setattr(sae, "analyze_σ_drivers", fake_analyze)
    monkeypatch.setattr(gate, "score", lambda _p, _r, **_k: (0.8, "RETHINK"))

    st.steer([0.0] * 6, "a", "b")
    st.steer([0.0] * 6, "c", "d")
    rep = st.report()
    assert rep["total_interventions"] == 2
    assert rep["suppressed"] == 1
    assert rep["amplified"] == 1
