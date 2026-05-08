# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

"""V2 tests for :mod:`cos.sae` — σ-driver lists, decode/reconstruct, dead features, Gini proxy."""
from __future__ import annotations

import sys
from pathlib import Path

import pytest

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

pytest.importorskip("numpy")

from cos.sae import SigmaSAE  # noqa: E402


class _PromptTagGate:
    def score(self, prompt: str, response: str):
        if "HIGH" in str(prompt):
            return 0.85, "ABSTAIN"
        return 0.15, "ACCEPT"


def test_encode_sparse() -> None:
    sae = SigmaSAE(input_dim=32, n_features=128, sparsity_k=8, seed=7)
    out = sae.encode([0.02 * i for i in range(32)])
    assert "sparse" in out and "active_features" in out
    assert len(out["sparse"]) == 128
    assert out["n_active"] == len(out["active_features"])
    assert out["n_active"] <= 8


def test_decode_reconstructs() -> None:
    sae = SigmaSAE(input_dim=16, n_features=48, sparsity_k=6, seed=11)
    vec = [0.1] * 16
    enc = sae.encode(vec, track_counts=False)
    recon = sae.decode(enc["sparse"])
    assert recon is not None
    assert recon.shape == (16,)
    assert float(recon.dot(recon)) > 0.0


def test_analyze_sigma_drivers() -> None:
    sae = SigmaSAE(input_dim=24, n_features=72, gate=_PromptTagGate(), seed=13)
    cases = [("HIGH path", "aa"), ("LOW path", "bb"), ("HIGH path", "cc")]
    r = sae.analyze_σ_drivers(cases)
    assert r.get("total_analyzed") == 3
    assert "hallucination_features" in r and "coherence_features" in r
    assert r["n_features_analyzed"] >= 1


def test_steer_modifies_activation() -> None:
    sae = SigmaSAE(input_dim=12, n_features=36, sparsity_k=5, seed=17)
    x = [0.05 * i for i in range(12)]
    y = sae.steer(x, feature_id=2, strength=0.5)
    assert list(y) != list(x)


def test_label_feature() -> None:
    sae = SigmaSAE(input_dim=8, n_features=32, seed=19)
    sae.label_feature(3, "golden_gate_bridge-ish")
    assert sae.feature_labels[3] == "golden_gate_bridge-ish"
    assert sae.feature_names[3] == "golden_gate_bridge-ish"


def test_dead_features_detected() -> None:
    sae = SigmaSAE(input_dim=4, n_features=8, sparsity_k=2, seed=23)
    for _ in range(3):
        sae.encode([0.1, 0.2, 0.3, 0.4], track_counts=True)
    d = sae.dead_features()
    assert d["n_dead"] >= 1
    assert len(d["dead"]) == d["n_dead"]
    assert d["utilization"] == round(1.0 - d["n_dead"] / 8, 4)


def test_monosemanticity_score() -> None:
    sae = SigmaSAE(input_dim=8, n_features=40, sparsity_k=3, seed=29)
    for t in range(15):
        sae.encode([((t + i) % 5) * 0.02 for i in range(8)], track_counts=True)
    g = sae.monosemanticity_score()
    assert 0.0 <= g <= 1.0
