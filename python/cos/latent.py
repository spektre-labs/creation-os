# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-latent — latent-reasoning *proxy*, ponder-style budgeting, and consistency checks.

This is a **lab harness**: multi-pass agreement + σ-gate aggregation + a simple
complexity heuristic for compute budgeting. It does **not** require latent-token
training (e.g. Coconut-style) or a trained halting head (e.g. ACT/PonderNet).

Literature (ACT, PonderNet, latent chain-of-thought) motivates the knobs; this
module **scores behaviour** (cross-sample consistency + gate) rather than
inspecting hidden states. Do not merge with measured harness headlines; see
``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import re
from typing import Any, List, Optional


class ConsistencyResult:
    """Outcome of :meth:`SigmaLatent.consistency_probe`."""

    __slots__ = (
        "sigma",
        "verdict",
        "consistency",
        "n_unique",
        "n_total",
        "per_response_sigma",
        "sigma_variance",
    )

    def __init__(
        self,
        sigma: float,
        verdict: str,
        consistency: float,
        n_unique: int,
        n_total: int,
        *,
        per_response_sigma: Optional[List[float]] = None,
        sigma_variance: float = 0.0,
    ) -> None:
        self.sigma = float(sigma)
        self.verdict = str(verdict)
        self.consistency = float(consistency)
        self.n_unique = int(n_unique)
        self.n_total = int(n_total)
        self.per_response_sigma: List[float] = list(per_response_sigma or [])
        self.sigma_variance = float(sigma_variance)

    def __repr__(self) -> str:
        return (
            f"ConsistencyResult(σ={self.sigma}, {self.verdict}, "
            f"consistency={self.consistency}, "
            f"unique={self.n_unique}/{self.n_total})"
        )

    def __bool__(self) -> bool:
        return self.verdict == "ACCEPT"


class PonderBudget:
    """Compute / pass budget derived from a prompt-complexity heuristic (ACT-style)."""

    __slots__ = (
        "passes",
        "model_tier",
        "verify",
        "consistency_check",
        "compute_multiplier",
    )

    def __init__(
        self,
        passes: int,
        model_tier: str,
        verify: bool,
        consistency_check: bool,
        compute_multiplier: float,
    ) -> None:
        self.passes = int(passes)
        self.model_tier = str(model_tier)
        self.verify = bool(verify)
        self.consistency_check = bool(consistency_check)
        self.compute_multiplier = float(compute_multiplier)

    def __repr__(self) -> str:
        return (
            f"PonderBudget(passes={self.passes}, "
            f"tier={self.model_tier}, "
            f"compute={self.compute_multiplier}x)"
        )


class SigmaLatent:
    """Proxy for latent-style reliability: multi-answer consistency + σ-gate + ponder budget."""

    def __init__(self, gate: Any = None, n_samples: int = 5) -> None:
        from cos.sigma_gate import SigmaGate

        self.gate = gate or SigmaGate()
        self.n_samples = int(n_samples)

    def consistency_probe(self, prompt: str, responses: List[str]) -> ConsistencyResult:
        """Score agreement across several answers to the same prompt (no model internals).

        Low cross-answer consistency and high average σ suggest unstable or
        shortcut behaviour; agreement with low σ supports acceptance.
        """
        if not responses:
            return ConsistencyResult(
                1.0,
                "ABSTAIN",
                0.0,
                0,
                0,
                per_response_sigma=[],
                sigma_variance=0.0,
            )

        n = len(responses)

        similarities: List[float] = []
        for i in range(n):
            for j in range(i + 1, n):
                similarities.append(self._text_similarity(responses[i], responses[j]))

        avg_similarity = sum(similarities) / max(len(similarities), 1)

        unique_core: set[str] = set()
        for r in responses:
            unique_core.add(self._extract_core_answer(r))

        uniqueness_ratio = len(unique_core) / n

        sigmas: List[float] = []
        for r in responses:
            s, _ = self.gate.score(str(prompt), str(r))
            sigmas.append(float(s))

        avg_sigma = sum(sigmas) / n
        sigma_variance = sum((s - avg_sigma) ** 2 for s in sigmas) / max(n, 1)

        consistency_sigma = 1.0 - avg_similarity
        variance_signal = min(1.0, sigma_variance * 10.0)

        total_sigma = (
            0.4 * consistency_sigma
            + 0.3 * avg_sigma
            + 0.2 * uniqueness_ratio
            + 0.1 * variance_signal
        )
        total_sigma = min(1.0, max(0.0, total_sigma))

        if total_sigma < 0.3:
            verdict = "ACCEPT"
        elif total_sigma > 0.8:
            verdict = "ABSTAIN"
        else:
            verdict = "RETHINK"

        return ConsistencyResult(
            round(total_sigma, 4),
            verdict,
            round(avg_similarity, 4),
            len(unique_core),
            n,
            per_response_sigma=sigmas,
            sigma_variance=round(sigma_variance, 6),
        )

    def ponder_budget(
        self,
        prompt: str,
        complexity_estimate: Optional[float] = None,
    ) -> PonderBudget:
        """Allocate passes and verification from a prompt-only complexity score (no training)."""
        if complexity_estimate is None:
            c = self._estimate_complexity(prompt)
        else:
            c = float(complexity_estimate)

        if c < 0.2:
            return PonderBudget(1, "fast", False, False, 1.0)
        if c < 0.5:
            return PonderBudget(2, "medium", True, False, 2.5)
        if c < 0.8:
            return PonderBudget(3, "deep", True, True, 5.0)
        return PonderBudget(5, "deep", True, True, 10.0)

    def _text_similarity(self, a: str, b: str) -> float:
        words_a = set(a.lower().split())
        words_b = set(b.lower().split())
        if not words_a or not words_b:
            return 0.0
        intersection = words_a & words_b
        union = words_a | words_b
        return len(intersection) / len(union)

    def _extract_core_answer(self, response: str) -> str:
        """Strip fluff; keep numbers or a short leading span for coarse equality."""
        numbers = re.findall(r"\b\d+\.?\d*\b", response)
        first_sentence = response.split(".")[0].strip().lower()
        if numbers:
            return " ".join(numbers)
        return first_sentence[:50]

    def _estimate_complexity(self, prompt: str) -> float:
        score = 0.0
        prompt_lower = prompt.lower()
        words = prompt.split()

        if len(words) > 30:
            score += 0.3
        elif len(words) > 15:
            score += 0.1

        reasoning = (
            "prove",
            "derive",
            "why",
            "how does",
            "compare",
            "analyze",
            "step by step",
            "todista",
            "perustele",
            "miksi",
        )
        score += min(0.5, sum(0.15 for r in reasoning if r in prompt_lower))

        deep = (
            "math",
            "physics",
            "theorem",
            "proof",
            "algorithm",
            "quantum",
            "topology",
            "irrationality",
            "contradiction",
        )
        score += min(0.3, sum(0.1 for d in deep if d in prompt_lower))

        return min(1.0, score)


__all__ = ["SigmaLatent", "ConsistencyResult", "PonderBudget"]
