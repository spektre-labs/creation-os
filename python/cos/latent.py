# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-latent — reasoning proxy, ponder budgeting, multi-answer checks, and hidden consistency.

This is a **lab harness**: cross-sample text consistency + σ-gate aggregation, optional
**INSIDE-style** hidden-vector covariance stress, and semantic bucketing entropy — without
claiming harness or silicon parity. See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import hashlib
import math
import re
import statistics
from typing import Any, Dict, List, Mapping, Optional, Sequence

__all__ = ["SigmaLatent", "ConsistencyResult", "PonderBudget"]


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
    """Latent-style reliability: text consistency + σ-gate + hidden-vector stress + ponder budget."""

    def __init__(self, gate: Any = None, n_samples: int = 5) -> None:
        from cos.sigma_gate import SigmaGate

        self.gate = gate or SigmaGate()
        self.n_samples = int(n_samples)

    def consistency_probe(self, prompt: str, responses: List[str]) -> ConsistencyResult:
        """Score agreement across several answers to the same prompt (no model internals)."""
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

    def consistency_score(self, hidden_states_per_sample: Sequence[Sequence[float]]) -> Dict[str, Any]:
        """Top eigenvalue proxy of covariance of mean-pooled hidden vectors (power iteration)."""
        vecs = [[float(x) for x in row] for row in hidden_states_per_sample if row]
        n = len(vecs)
        if n < 2:
            return {"score": 0.0, "eigenvalue_proxy": 0.0}
        d = len(vecs[0])
        mu = [sum(vecs[i][j] for i in range(n)) / n for j in range(d)]
        centered = [[vecs[i][j] - mu[j] for j in range(d)] for i in range(n)]
        v = [1.0 / math.sqrt(d)] * d
        lam = 0.0
        for _ in range(16):
            tmp = [0.0] * d
            for row in centered:
                dot = sum(row[j] * v[j] for j in range(d))
                for j in range(d):
                    tmp[j] += dot * row[j]
            for j in range(d):
                tmp[j] /= float(max(n - 1, 1))
            norm = math.sqrt(sum(x * x for x in tmp)) or 1.0
            v = [x / norm for x in tmp]
            lam = sum(v[j] * tmp[j] for j in range(d))
        stress = min(1.0, max(0.0, abs(lam) / max(d, 1) ** 0.5))
        return {"score": round(stress, 6), "eigenvalue_proxy": round(float(lam), 6)}

    @staticmethod
    def semantic_clustering(responses: Sequence[str]) -> Dict[str, List[int]]:
        """Bucket by stable hash of sorted token multiset (no embedding model)."""
        buckets: Dict[str, List[int]] = {}
        for idx, r in enumerate(responses):
            toks = sorted(t.lower() for t in str(r).split())
            key = hashlib.sha256(" ".join(toks).encode()).hexdigest()[:16]
            buckets.setdefault(key, []).append(idx)
        return buckets

    @staticmethod
    def semantic_entropy(clusters: Mapping[str, Sequence[int]]) -> float:
        """Normalized Shannon entropy of cluster occupancy."""
        counts = [len(v) for v in clusters.values() if v]
        if not counts:
            return 0.0
        tot = sum(counts) or 1
        h = 0.0
        for c in counts:
            p = c / tot
            h -= p * math.log(p + 1e-12)
        denom = math.log(len(counts) + 1e-12 + 1)
        return float(min(1.0, h / denom)) if denom > 0 else 0.0

    def sigma_from_latent(self, consistency: float, semantic_entropy: float) -> float:
        """Both low ⇒ trustworthy (mapped to low σ); both high ⇒ stress."""
        c = float(consistency)
        e = float(semantic_entropy)
        return float(max(0.0, min(1.0, 0.55 * c + 0.45 * e)))

    def single_pass_approximation(self, hidden_state: Sequence[float], sep_probe: Any) -> Dict[str, Any]:
        """Delegate to ``sep_probe.score_hidden_vector(vec)`` if present, else mean-abs proxy."""
        vec = [float(x) for x in hidden_state]
        fn = getattr(sep_probe, "score_hidden_vector", None)
        if callable(fn):
            s = float(fn(vec))
        else:
            s = min(1.0, statistics.mean(abs(x) for x in vec) if vec else 0.5)
        return {"sigma_proxy": round(s, 6), "mode": "sep" if callable(fn) else "norm"}

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
