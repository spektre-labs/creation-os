# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.summary`."""
from __future__ import annotations

from cos.sigma_gate import SigmaGate
from cos.summary import SigmaSummary


def test_score_summary() -> None:
    s = SigmaSummary()
    src = "The Company Acme reported revenue of ten in Q1."
    summ = "Acme reported revenue of ten in Q1."
    r = s.score_summary(src, summ, SigmaGate())
    assert "sigma_combined" in r


def test_extractive_check() -> None:
    s = SigmaSummary()
    src = "Alpha beta gamma."
    out = s.extractive_check("Alpha beta.", src)
    assert out["extractive_ratio"] >= 0.0


def test_entity_check_novel() -> None:
    s = SigmaSummary()
    n = s.entity_check("Visit Acme Corp today.", "We sell bread.")
    assert isinstance(n["novel_entities"], list)


def test_length_ratio() -> None:
    r = SigmaSummary.length_ratio("short", "x" * 1000)
    assert "ratio" in r


def test_multi_doc_sigma() -> None:
    s = SigmaSummary()
    r = s.multi_doc_sigma(["a"], ["longer source text here"], SigmaGate())
    assert r["ok"] is True and "mean_sigma" in r
