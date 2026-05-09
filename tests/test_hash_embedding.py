# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for SIF-weighted hash pseudo-embeddings (:class:`~cos.probes.HashEmbedding`)."""
from __future__ import annotations

from cos.probes import HashEmbedding


def test_same_text_high_similarity() -> None:
    s = "the cat sat on the mat"
    sim = HashEmbedding.similarity(s, s)
    assert sim > 0.99


def test_similar_text_moderate() -> None:
    sim = HashEmbedding.similarity(
        "What is the capital of France?",
        "Paris is the capital of France",
    )
    assert sim > 0.3


def test_unrelated_text_low() -> None:
    sim = HashEmbedding.similarity("What is 2+2?", "banana smoothie recipe")
    assert sim < 0.5


def test_sif_weights_rare_words_higher() -> None:
    w_the = HashEmbedding.sif_weight("the")
    w_shakespeare = HashEmbedding.sif_weight("shakespeare")
    assert w_shakespeare > w_the


def test_word_vec_deterministic() -> None:
    a = HashEmbedding.word_to_vec("hello")
    b = HashEmbedding.word_to_vec("hello")
    assert a == b
