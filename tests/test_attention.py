# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import sys
from pathlib import Path

import pytest

pytest.importorskip("numpy")

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.attention import AttentionAnalyzer  # noqa: E402


def test_matching_QK_low_sigma() -> None:
    an = AttentionAnalyzer()
    qk = [10.0, 10.0, 10.0]
    out = an.attention_σ(qk, qk)
    assert out["σ"] < 0.3
    assert out["coherent"] is True
    assert "1=1" in out["interpretation"]


def test_mismatching_QK_high_sigma() -> None:
    an = AttentionAnalyzer()
    out = an.attention_σ([3.0, 0.0], [-3.0, 0.0])
    assert out["σ"] > 0.7
    assert out["coherent"] is False
    assert "1≠1" in out["interpretation"]


def test_sparsity_focused_low_sigma() -> None:
    an = AttentionAnalyzer()
    # Peaked mass ≈ one-hot — high Gini / focused
    mat = [[0.97, 0.015, 0.015]]
    out = an.sparsity_σ(mat)
    assert out["focused"] is True
    dense = an.sparsity_σ([[1.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0]])
    assert out["σ"] < dense["σ"]


def test_sparsity_dense_high_sigma() -> None:
    an = AttentionAnalyzer()
    u = 1.0 / 4.0
    out = an.sparsity_σ([[u, u, u, u]])
    assert out["focused"] is False
    assert out["σ"] > 0.5


def test_head_coherence_agreement() -> None:
    an = AttentionAnalyzer()
    out = an.head_coherence([[1.0, 2.0, 3.0], [2.0, 4.0, 6.0]])
    assert out["head_agreement"] == pytest.approx(1.0, abs=1e-4)
    assert out["σ"] < 0.3
    assert out["coherent"] is True


def test_head_coherence_disagreement() -> None:
    an = AttentionAnalyzer()
    out = an.head_coherence([[1.0, 0.0], [-1.0, 0.0]])
    assert out["head_agreement"] == pytest.approx(-1.0, abs=1e-4)
    assert out["σ"] > 0.5
    assert out["coherent"] is False
