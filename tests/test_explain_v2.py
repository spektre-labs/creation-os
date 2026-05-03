# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.explain` (v2)."""
from __future__ import annotations

from cos.explain import SigmaExplain
from cos.sigma_gate import SigmaGate


def test_explain_natural_language() -> None:
    e = SigmaExplain(SigmaGate())
    r = e.explain("p", "r", 0.2, "ACCEPT")
    assert r["natural_language"] is True and "gate" in r["summary"].lower()


def test_counterfactual_changes_sigma() -> None:
    e = SigmaExplain(SigmaGate())
    r = e.counterfactual("What is 2+2?", "The answer is four")
    assert "original_sigma" in r and "counterfactual_sigma" in r


def test_token_attribution_lists_tokens() -> None:
    e = SigmaExplain(SigmaGate())
    r = e.token_attribution("a b c")
    assert len(r["per_token"]) == 3


def test_feature_importance_top() -> None:
    e = SigmaExplain()
    r = e.feature_importance({"icr": 0.2, "spectral": 0.9})
    assert r["top"]["name"] == "spectral"


def test_contrastive_two_responses() -> None:
    e = SigmaExplain(SigmaGate())
    r = e.contrastive("four", "purple elephant")
    assert "good" in r and "bad" in r


def test_confidence_in_explanation_bounds() -> None:
    e = SigmaExplain()
    r = e.confidence_in_explanation(0.1, 0.5)
    assert 0.0 <= r["confidence"] <= 1.0
