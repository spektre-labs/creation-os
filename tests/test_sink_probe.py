# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for ``cos.sink_probe`` (attention sink lab, no torch required)."""
from __future__ import annotations

from cos.cascade import cascade_L6
from cos.sink_probe import SigmaSinkProbe


def _uniform_attn(n: int = 5):
    u = 1.0 / n
    mat = [[u] * n for _ in range(n)]
    return [[mat]]


def _sink_last_attn(n: int = 5):
    mat = []
    for _ in range(n):
        row = [0.05 / max(n - 1, 1)] * n
        row[-1] = 0.95
        s = sum(row)
        mat.append([x / s for x in row])
    return [[mat]]


def test_sigma_sink_probe_training_free() -> None:
    p = SigmaSinkProbe()
    assert p.training_free is True


def test_sink_score_uniform_low_aggregate() -> None:
    p = SigmaSinkProbe()
    out = p.sink_score(_uniform_attn(6))
    assert out["n_tokens"] == 6
    assert out["aggregate"] < 0.15


def test_sink_score_column_sink_raises_aggregate() -> None:
    p = SigmaSinkProbe()
    uni = p.sink_score(_uniform_attn(5))["aggregate"]
    sk = p.sink_score(_sink_last_attn(5))["aggregate"]
    assert sk > uni


def test_compression_valley_detect_mid_layer() -> None:
    p = SigmaSinkProbe()
    # Layer L2 norms drop sharply in the middle then flatten.
    hs = [[1.0], [0.98], [0.4], [0.39], [0.38]]
    out = p.compression_valley_detect(hs)
    assert out["index"] >= 0
    assert out["score"] > 0.2
    assert len(out["deltas"]) == 4


def test_combined_with_spectral_keys() -> None:
    p = SigmaSinkProbe()
    c = p.combined_with_spectral(_sink_last_attn(4))
    assert "sink_aggregate" in c and "spectral_stress" in c and "combined" in c
    assert len(c["per_token"]) == 4


def test_cascade_l6_with_attention_maps() -> None:
    v = cascade_L6(_sink_last_attn(5), hidden_states=None)
    assert 0.0 <= v <= 1.0
