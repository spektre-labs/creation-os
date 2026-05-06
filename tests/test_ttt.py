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
    """σ average drops when σ tracks tightened accept band (test double)."""

    class _TunableVolGate:
        threshold_accept = 0.5
        threshold_abstain = 0.9

        def score(self, p: str, r: str) -> tuple[float, str]:
            _ = (p, r)
            s = max(0.0, float(self.threshold_accept) - 0.2)
            if s < self.threshold_accept:
                v = "ACCEPT"
            elif s < self.threshold_abstain:
                v = "RETHINK"
            else:
                v = "ABSTAIN"
            return s, v

        def adjust_threshold(self, band: str, delta: float, *, floor: float = 0.02, ceil: float = 0.99) -> None:
            step = float(delta)
            key = str(band).lower()
            eps = 0.01
            if key in ("accept", "threshold_accept", "a"):
                v = float(self.threshold_accept) + step
                self.threshold_accept = max(floor, min(v, float(self.threshold_abstain) - eps))
            elif key in ("abstain", "threshold_abstain", "ab"):
                v = float(self.threshold_abstain) + step
                self.threshold_abstain = min(ceil, max(v, float(self.threshold_accept) + eps))
            else:
                raise ValueError(band)

    g = _TunableVolGate()
    out = SigmaTTT().adapt([("p", "r", False)], g, chunks=1)
    assert out["improved"] is True


def test_wrong_accept_tightens_threshold() -> None:
    g = SigmaGate(threshold_accept=0.5, threshold_abstain=0.9)
    before = g.threshold_accept
    SigmaTTT().adapt([("hello", "world", False)], g, chunks=1)
    assert g.threshold_accept < before


def test_correct_abstain_loosens_threshold() -> None:
    g = SigmaGate(threshold_accept=0.1, threshold_abstain=0.2)
    before = g.threshold_abstain
    SigmaTTT().adapt([("a", "b", True)], g, chunks=1)
    assert g.threshold_abstain > before


def test_empty_examples_returns_zero() -> None:
    out = SigmaTTT().adapt([], SigmaGate(), chunks=3)
    assert out["examples"] == 0
    assert out["σ_before_avg"] == 0.0 and out["σ_after_avg"] == 0.0


def test_before_after_sigma_reported() -> None:
    out = SigmaTTT().adapt([("openai", "gpt", False)], SigmaGate(), chunks=2)
    assert "σ_before_avg" in out and "σ_after_avg" in out
