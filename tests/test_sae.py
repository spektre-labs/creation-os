# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.sae` lab SAE scaffold (NumPy-backed)."""
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
        if "HIGH_SIGMA" in str(prompt):
            return 0.85, "ABSTAIN"
        return 0.15, "ACCEPT"


def test_encode_returns_sparse_features() -> None:
    sae = SigmaSAE(input_dim=64, hidden_dim=256, seed=42)
    vec = [0.1 * i for i in range(64)]
    out = sae.encode(vec)
    assert out["n_active"] >= 1
    assert isinstance(out["features"], list)
    assert all(len(t) == 2 for t in out["features"])


def test_sparsity_high() -> None:
    sae = SigmaSAE(input_dim=64, hidden_dim=256, seed=1)
    out = sae.encode([0.5] * 64)
    assert out["sparsity"] >= 0.85


def test_analyze_sigma_drivers() -> None:
    sae = SigmaSAE(input_dim=32, hidden_dim=128, gate=_PromptTagGate(), seed=3)
    cases = [("HIGH_SIGMA", "alpha"), ("LOW_SIGMA", "beta")]
    r = sae.analyze_sigma_drivers(cases)
    assert r["n_features_analyzed"] >= 1
    assert "all" in r and len(r["all"]) >= 1


def test_hallucination_features_identified() -> None:
    sae = SigmaSAE(input_dim=32, hidden_dim=128, gate=_PromptTagGate(), seed=4)
    shared = "probe-text"
    cases = [("HIGH_SIGMA", shared)] * 4 + [("LOW_SIGMA", shared)]
    r = sae.analyze_sigma_drivers(cases)
    assert len(r["hallucination_features"]) >= 1


def test_steer_modifies_activation() -> None:
    sae = SigmaSAE(input_dim=16, hidden_dim=48, seed=5)
    x = [0.1] * 16
    y = sae.steer(x, feature_id=3, strength=2.0)
    assert list(y) != list(x)


def test_text_to_vector() -> None:
    sae = SigmaSAE(input_dim=64, hidden_dim=16, seed=6)
    v = sae._text_to_vector("abc")
    assert len(v) == 64
    assert all(0.0 <= float(t) <= 1.0 for t in v)
