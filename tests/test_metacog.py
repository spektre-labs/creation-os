# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from cos.metacog import SigmaMetacog, UncertaintyType


def test_low_uncertainty() -> None:
    meta = SigmaMetacog()
    result = meta.assess(
        "What is 2+2?",
        "The answer is 4.",
        sigma=0.05,
    )
    assert result["uncertainty_type"] == UncertaintyType.LOW
    assert result["action"] == "respond"


def test_epistemic_uncertainty() -> None:
    meta = SigmaMetacog()
    result = meta.assess(
        "What was the GDP of Tuvalu in Q3 2025?",
        "I'm not sure, but I think it might be around...",
        sigma=0.7,
        signals={"logprob_mean": -3.5},
    )
    assert result["epistemic_sigma"] > 0.3
    assert result["action"] in ("retrieve", "abstain")


def test_aleatoric_uncertainty() -> None:
    meta = SigmaMetacog()
    result = meta.assess(
        "What about it?",
        "Could you clarify what you mean?",
        sigma=0.3,
    )
    assert result["aleatoric_sigma"] > 0.3
    assert result["action"] == "clarify"


def test_hedging_detection() -> None:
    meta = SigmaMetacog()
    result = meta.assess(
        "Is X true?",
        "Maybe, perhaps, I think it could possibly be true, but I'm not sure",
        sigma=0.5,
    )
    assert result["epistemic_sigma"] > 0.3


def test_reflect_updates_threshold() -> None:
    meta = SigmaMetacog()
    initial = meta.epistemic_threshold

    assessment = meta.assess("test", "wrong answer", sigma=0.1)
    meta.reflect(assessment, {"correct": False})

    assert meta.epistemic_threshold < initial


def test_stats() -> None:
    meta = SigmaMetacog()
    meta.assess("test1", "response1", sigma=0.1)
    meta.assess("test2", "maybe response", sigma=0.6)

    stats = meta.get_stats()
    assert stats["total"] == 2
    assert "avg_epistemic" in stats


def test_mixed_uncertainty() -> None:
    meta = SigmaMetacog(epistemic_threshold=0.3, aleatoric_threshold=0.3)
    result = meta.assess(
        "What?",
        "I think maybe perhaps it could be something, not sure",
        sigma=0.8,
        signals={"logprob_mean": -4.0},
    )
    assert result["total_sigma"] > 0.3


def test_total_sigma_bounded() -> None:
    meta = SigmaMetacog()
    result = meta.assess("test", "test", sigma=0.99, signals={"logprob_mean": -10.0})
    assert 0.0 <= result["total_sigma"] <= 1.0
