# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Unit tests for σ-gate L1–L5 probes and :class:`~cos.probes.SigmaFusion`."""
from __future__ import annotations

from cos.probes import (
    L1EntropyProbe,
    L2LogProbVariance,
    L3HiddenStateDivergence,
    L4MultiTokenAggregation,
    L5SpectralSignature,
    SigmaFusion,
)


def test_l1_normal_text_low_sigma() -> None:
    p = L1EntropyProbe()
    s = p.score("What is the capital of France?", "Paris is the capital of France.")
    assert s < 0.35


def test_l1_repetitive_text_high_sigma() -> None:
    p = L1EntropyProbe()
    s = p.score("x", "aaaaaa")
    assert s > 0.45


def test_l2_related_prompt_response_low() -> None:
    l2 = L2LogProbVariance()
    prompt = "Explain photosynthesis in one sentence."
    response = "Photosynthesis converts light into chemical energy in plants."
    assert l2.score(prompt, response) < 0.5


def test_l2_unrelated_high() -> None:
    l2 = L2LogProbVariance()
    prompt = "What is the capital of France?"
    response = "xyzzy qwerty unrelated_token"  # deliberately far from the prompt
    assert l2.score(prompt, response) >= 0.5


def test_l3_ngram_similar_low() -> None:
    l3 = L3HiddenStateDivergence()
    text = "the quick brown fox jumps over the lazy dog"
    s = l3.score(text, text)
    assert s < 0.2


def test_l3_ngram_divergent_high() -> None:
    l3 = L3HiddenStateDivergence()
    s = l3.score("alpha beta gamma delta epsilon", "zed yax wov uts qix")
    assert s > 0.5


def test_l4_late_onset_drift_detected() -> None:
    l4 = L4MultiTokenAggregation()
    prompt = "Summarize the treaty."
    response = (
        "The treaty established trade routes. "
        "The treaty established trade routes. "
        "Quantum bananas violate causality in dimension seven."
    )
    s = l4.score(prompt, response)
    assert s >= 0.5


def test_l4_stable_response_low() -> None:
    l4 = L4MultiTokenAggregation()
    prompt = "What is 2+2?"
    response = "Two plus two equals four."
    assert l4.score(prompt, response) < 0.45


def test_l5_repetition_detected() -> None:
    l5 = L5SpectralSignature()
    text = "Hello. Hello. Hello. Hello."
    assert l5.score("Say hello once.", text) >= 0.4


def test_l5_contradiction_detected() -> None:
    l5 = L5SpectralSignature()
    r = "The answer is yes. The answer is not yes."
    assert l5.score("Is it true?", r) >= 0.35


def test_fusion_good_response_accept() -> None:
    fusion = SigmaFusion()
    line = "Paris is the capital of France."
    sigma, verdict, _parts = fusion.score(
        line,
        line,
        per_token_sigma=[0.05] * 12,
    )
    assert sigma < 0.15
    assert verdict == "ACCEPT"


def test_fusion_bad_response_abstain() -> None:
    fusion = SigmaFusion()
    prompt = "Name one prime number."
    response = "banana banana banana banana" * 10
    sigma, verdict, _parts = fusion.score(prompt, response)
    assert sigma >= 0.5
    assert verdict == "ABSTAIN"
