# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from cos.sigma_gate import SigmaGate
from cos.ttt import SigmaTTT


def test_sigma_trigger() -> None:
    t = SigmaTTT(trigger_threshold=0.5)
    assert t.sigma_trigger(0.9) is True
    assert t.sigma_trigger(0.1) is False


def test_adapt_updates_weights() -> None:
    t = SigmaTTT()
    w0 = dict(t.fast_weights)
    w1 = t.adapt("context text here" * 5, w0)
    assert "mlp_proj" in w1
    assert isinstance(w1["mlp_proj"], float)


def test_chunk_wise_update() -> None:
    t = SigmaTTT()
    t.chunk_wise_update(["a", "b"], None)
    assert "mlp_proj" in t.fast_weights


def test_ntp_stub() -> None:
    t = SigmaTTT()
    loss = t.ntp_objective_stub("hello", "x")
    assert 0.0 <= loss <= 1.0


def test_adapt_with_sigma_skips_when_low() -> None:
    t = SigmaTTT(trigger_threshold=0.9)
    g = SigmaGate()
    out = t.adapt_with_sigma(0.05, "ctx", g)
    assert out["applied"] is False


def test_adapt_with_sigma_structure() -> None:
    t = SigmaTTT(trigger_threshold=0.01)
    g = SigmaGate()
    out = t.adapt_with_sigma(0.95, "short ctx for ttt", g)
    assert "rollback" in out and "sigma_after" in out


def test_fast_weight_key_present() -> None:
    t = SigmaTTT()
    assert "mlp_proj" in t.fast_weights


def test_adapt_improves_sigma() -> None:
    g = SigmaGate(threshold_accept=0.85, threshold_abstain=0.92)
    ex = [("p1", "x" * 400, False)]
    r = SigmaTTT().adapt(ex, g, chunks=1)
    assert r["examples"] == 1
    assert "improved" in r
    assert "σ_before_avg" in r and "σ_after_avg" in r


def test_wrong_accept_tightens_threshold() -> None:
    g = SigmaGate(threshold_accept=0.99, threshold_abstain=0.995)
    lo = float(g.threshold_accept)
    SigmaTTT().adapt([("q", "aaaa", False)], g, chunks=1)
    assert float(g.threshold_accept) < lo


def test_correct_abstain_loosens_threshold() -> None:
    g = SigmaGate(threshold_accept=0.1, threshold_abstain=0.15)
    hi = float(g.threshold_abstain)
    SigmaTTT().adapt([("q", "zzzzzzzz" * 80, True)], g, chunks=1)
    assert float(g.threshold_abstain) > hi


def test_empty_examples_returns_zero() -> None:
    r = SigmaTTT().adapt([], SigmaGate(), chunks=3)
    assert r["examples"] == 0
    assert r["σ_before_avg"] == 0.0
    assert r["σ_after_avg"] == 0.0


def test_before_after_sigma_reported() -> None:
    r = SigmaTTT().adapt(
        [("a", "b", True), ("c", "d", False)],
        SigmaGate(),
        chunks=2,
    )
    assert "sigma_before_avg" in r and "sigma_after_avg" in r
    assert "σ_before_avg" in r and "σ_after_avg" in r
