# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from cos.latent import ConsistencyResult, PonderBudget, SigmaLatent


def test_consistency_identical() -> None:
    lat = SigmaLatent()
    responses = ["The answer is 4", "The answer is 4", "The answer is 4"]
    result = lat.consistency_probe("What is 2+2?", responses)
    assert result.consistency > 0.5
    assert result.n_unique <= 2


def test_consistency_divergent() -> None:
    lat = SigmaLatent()
    responses = [
        "The capital is Paris",
        "I believe it might be London",
        "Berlin is the capital",
        "The answer is Madrid",
    ]
    result = lat.consistency_probe("What is the capital of France?", responses)
    assert result.n_unique >= 3
    assert result.sigma > 0.3


def test_consistency_empty() -> None:
    lat = SigmaLatent()
    result = lat.consistency_probe("test", [])
    assert result.sigma == 1.0
    assert result.verdict == "ABSTAIN"


def test_ponder_simple() -> None:
    lat = SigmaLatent()
    budget = lat.ponder_budget("What is 2+2?")
    assert budget.passes <= 2
    assert budget.model_tier == "fast"


def test_ponder_complex() -> None:
    lat = SigmaLatent()
    budget = lat.ponder_budget(
        "Prove the irrationality of sqrt(2) step by step using proof by contradiction",
    )
    assert budget.passes >= 3
    assert budget.model_tier == "deep"
    assert budget.verify is True


def test_ponder_budget_repr() -> None:
    b = PonderBudget(3, "deep", True, True, 5.0)
    assert "deep" in repr(b)
    assert "5.0" in repr(b)


def test_consistency_result_bool() -> None:
    r = ConsistencyResult(0.1, "ACCEPT", 0.9, 1, 5)
    assert bool(r) is True
    r2 = ConsistencyResult(0.9, "ABSTAIN", 0.1, 5, 5)
    assert bool(r2) is False


def test_per_response_sigma() -> None:
    lat = SigmaLatent()
    responses = ["hello", "world", "test"]
    result = lat.consistency_probe("test", responses)
    assert len(result.per_response_sigma) == 3


def test_hidden_consistency_score_similar_vectors() -> None:
    lat = SigmaLatent()
    hs = [[0.1, 0.2, 0.3], [0.11, 0.19, 0.31], [0.12, 0.21, 0.29]]
    out = lat.consistency_score(hs)
    assert out["score"] < 0.05


def test_semantic_clustering_groups_paraphrases() -> None:
    lat = SigmaLatent()
    clusters = lat.semantic_clustering(["a b c", "c b a", "x y z"])
    assert len(clusters) == 2
    assert sum(len(v) for v in clusters.values()) == 3


def test_semantic_entropy_normalized() -> None:
    lat = SigmaLatent()
    clusters = {"a": [0], "b": [1, 2]}
    h = lat.semantic_entropy(clusters)
    assert 0.0 <= h <= 1.0


def test_sigma_from_latent_blend() -> None:
    lat = SigmaLatent()
    s = lat.sigma_from_latent(0.8, 0.2)
    assert 0.45 <= s <= 0.55


class _SepStub:
    @staticmethod
    def score_hidden_vector(vec: list[float]) -> float:
        return float(sum(vec) / max(len(vec), 1))


def test_single_pass_approximation_sep() -> None:
    lat = SigmaLatent()
    out = lat.single_pass_approximation([0.2, 0.2, 0.2], _SepStub())
    assert out["mode"] == "sep"
    assert abs(out["sigma_proxy"] - 0.2) < 1e-5


def test_single_pass_approximation_norm_fallback() -> None:
    lat = SigmaLatent()
    out = lat.single_pass_approximation([1.0, -1.0], object())
    assert out["mode"] == "norm"
    assert out["sigma_proxy"] == 1.0
