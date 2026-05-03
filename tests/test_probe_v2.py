# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from cos.cascade import cascade_L2, cascade_L3, cascade_L4, cascade_L5
from cos.probe import (
    EnergyProbe,
    ICRProbe,
    SEPProbe,
    SignalCascade,
    SpectralProbe,
    ensemble_score,
)
from cos.sigma_gate import SigmaGate


def test_icr_cross_layer_variance() -> None:
    hs = [[[0.0, 1.0], [0.1, 0.9]], [[0.5, 0.5], [0.6, 0.4]], [[1.0, 0.0], [1.1, -0.1]]]
    s = ICRProbe().score(hs)
    assert 0.0 <= s <= 1.0


def test_sep_pseudo_entropy_and_fit() -> None:
    sp = SEPProbe()
    assert 0.0 <= sp.pseudo_semantic_entropy(["a b", "a c"]) <= 1.0
    hvecs = [[[1.0]], [[0.8]], [[0.6]]]
    texts = [["x y", "x z"], ["a"], ["b", "b", "b"]]
    sp.fit_from_samples(hvecs, texts)
    assert sp.fitted is True


def test_spectral_attention_and_stack() -> None:
    attn = [[0.7, 0.2], [0.2, 0.7]]
    a = SpectralProbe.score_attention_matrix(attn)
    assert 0.0 <= a <= 1.0
    hs = [[[1.0, 0.0], [0.0, 1.0]], [[0.9, 0.1], [0.1, 0.9]]]
    b = SpectralProbe.score_hidden_stack(hs)
    assert 0.0 <= b <= 1.0


def test_energy_probe_tensor_stub() -> None:
    e = EnergyProbe.score_hidden_tensor([0.2, -0.4, 0.1])
    assert 0.0 <= e <= 1.0


def test_ensemble_score_weighted() -> None:
    out = ensemble_score({"L1_entropy": 0.2, "L2_hide": 0.8, "ICR": 0.4})
    assert "sigma_ensemble" in out


def test_cascade_functions_match_signalcascade_hs() -> None:
    hs = [[[0.0], [1.0]], [[0.2], [0.8]], [[0.3], [0.7]]]
    assert 0.0 <= cascade_L2(hs) <= 1.0
    assert 0.0 <= cascade_L3(hs) <= 1.0
    assert 0.0 <= cascade_L4(hs) <= 1.0
    assert 0.0 <= cascade_L5(hs) <= 1.0


def test_sigma_gate_score_cascade_imports_cos_cascade() -> None:
    g = SigmaGate()
    hs = [[[0.1], [0.2]], [[0.3], [0.4]], [[0.2], [0.3]]]
    d = g.score_cascade("q", "answer text here", hidden_states=hs)
    assert "L2_hide" in d["levels"] or "L1_entropy" in d["levels"]
    assert "sigma" in d


def test_signal_cascade_public_layer_list() -> None:
    sc = SignalCascade()
    assert len(sc.as_layer_list([[[1.0]], [[2.0]]])) == 2
