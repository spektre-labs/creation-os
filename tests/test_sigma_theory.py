# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.theory.sigma_theory` (corpus / pedagogical formalism)."""
from __future__ import annotations

from cos.theory.sigma_theory import SigmaTheory


def test_godel_boundary_transparent() -> None:
    assert "transparent" in SigmaTheory.godel_boundary(0.1)


def test_godel_boundary_opaque() -> None:
    assert "opaque" in SigmaTheory.godel_boundary(0.95)


def test_resolve_1_equals_1_improves() -> None:
    r = SigmaTheory.resolve_1_equals_1(0.5, 0.6)
    assert r["resolution"] is True
    assert r["σ_combined"] < min(r["σ_a"], r["σ_b"])


def test_levels_all_present() -> None:
    for key in ("human", "machine", "institution", "biology"):
        assert key in SigmaTheory.LEVELS
        assert "σ_source" in SigmaTheory.LEVELS[key]
        assert "boundary_behavior" in SigmaTheory.LEVELS[key]
        assert "resolution" in SigmaTheory.LEVELS[key]


def test_why_not_iit_structure() -> None:
    r = SigmaTheory.why_not_iit()
    assert "IIT" in r
    assert "sigma" in r
    assert r["IIT"]["axioms"] == 5
    assert r["IIT"]["computable"] is False
    assert r["sigma"]["axioms"] == 0
    assert r["sigma"]["computable"] is True
    assert r["sigma"]["variables"] == 1
