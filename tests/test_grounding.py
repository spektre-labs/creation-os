# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for ``cos.grounding``."""
from __future__ import annotations

from cos.grounding import SigmaGrounding


def test_ground_check_returns_sigma_verdict() -> None:
    g = SigmaGrounding()
    r = g.ground_check("q", "a", "")
    assert "sigma" in r and "verdict" in r
    assert 0.0 <= r["sigma"] <= 1.0


def test_attention_to_context_ratio_high_on_context() -> None:
    g = SigmaGrounding()
    n, ctx = 6, 4
    mat = []
    for _ in range(n):
        row = [0.0] * n
        for j in range(ctx):
            row[j] = 1.0 / ctx
        mat.append(row)
    attn = [[mat]]
    ratio = g.attention_to_context_ratio(attn, context_len=ctx, seq_len=n)
    assert ratio > 0.95


def test_attention_to_context_ratio_zero_when_maps_empty() -> None:
    g = SigmaGrounding()
    assert g.attention_to_context_ratio(None, context_len=3) == 0.0


def test_ffn_activation_score() -> None:
    g = SigmaGrounding()
    assert g.ffn_activation_score([0.0, 2.0, -1.0]) > 0.9
    assert g.ffn_activation_score([]) == 0.0


def test_source_attribution_best_chunk() -> None:
    g = SigmaGrounding()
    r = g.source_attribution(
        "Paris is the capital of France.",
        ["London is large.", "France has Paris as capital."],
    )
    assert r["best_chunk"] == 1
    assert r["score"] > 0.2


def test_ungrounded_spans_flags_missing_overlap() -> None:
    g = SigmaGrounding()
    r = g.ungrounded_spans(
        "Alpha beta. Gamma delta epsilon.",
        context="alpha beta",
        min_words=2,
    )
    assert r["count"] >= 1
    assert any("gamma" in s.lower() for s in r["ungrounded"])
