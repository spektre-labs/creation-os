# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# Source:        https://github.com/spektre-labs/creation-os-kernel
# Website:       https://spektrelabs.org
# Commercial:    spektre.labs@proton.me
# License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
"""σ-gate L1–L5 probes: lightweight, reference-free detection signals for ``SigmaGate``.

Each probe returns σ ∈ [0, 1]. :class:`SigmaFusion` combines them with fixed weights.
Optional ``logprobs``, ``embeddings``, and ``attention`` improve scoring when available;
pure-Python fallbacks require no extra dependencies.
"""
from __future__ import annotations

import math
from typing import Dict, Mapping, MutableMapping, Optional, Sequence, Tuple


class L1EntropyProbe:
    """Token-level entropy proxy from character distribution. Fast and always available."""

    def score(self, prompt: str, response: str) -> float:
        del prompt
        if not response:
            return 0.5
        freq: dict[str, int] = {}
        for c in response:
            freq[c] = freq.get(c, 0) + 1
        n = len(response)
        entropy = -sum(
            (count / n) * math.log2(count / n) for count in freq.values() if count > 0
        )
        normalized = min(entropy / 5.0, 1.0)
        if normalized < 0.3:
            return 0.7
        if normalized > 0.9:
            return 0.6
        return 0.2


class L2LogProbVariance:
    """Variance of per-token log-probabilities (reference-free). Falls back to text heuristics."""

    def score(
        self,
        prompt: str,
        response: str,
        logprobs: Optional[Sequence[float]] = None,
    ) -> float:
        if logprobs is None:
            return self._estimate_from_text(prompt, response)
        lp_list = list(logprobs)
        if len(lp_list) < 2:
            return 0.5
        mean_lp = sum(lp_list) / len(lp_list)
        variance = sum((lp - mean_lp) ** 2 for lp in lp_list) / len(lp_list)
        return float(min(1.0, variance / 5.0))

    def _estimate_from_text(self, prompt: str, response: str) -> float:
        p_words = set(prompt.lower().split())
        r_words = set(response.lower().split())
        if not r_words:
            return 0.8
        overlap = len(p_words & r_words) / max(len(r_words), 1)
        length_ratio = len(response) / max(len(prompt), 1)

        sigma = 0.3
        if overlap < 0.05:
            sigma += 0.3
        if length_ratio > 10:
            sigma += 0.2
        if length_ratio < 0.1:
            sigma += 0.1
        return float(min(1.0, sigma))


class L3HiddenStateDivergence:
    """Semantic divergence (HIDE): embedding cosine distance or n-gram Jaccard fallback."""

    def score(
        self,
        prompt: str,
        response: str,
        embeddings: Optional[Mapping[str, Sequence[float]]] = None,
    ) -> float:
        if embeddings:
            return self._embedding_score(embeddings)
        return self._ngram_divergence(prompt, response)

    def _ngram_divergence(self, prompt: str, response: str, n: int = 3) -> float:
        def ngrams(text: str, ngram: int) -> set[tuple[str, ...]]:
            words = text.lower().split()
            if len(words) < ngram:
                return set()
            return {tuple(words[i : i + ngram]) for i in range(len(words) - ngram + 1)}

        p_ng = ngrams(prompt, n)
        r_ng = ngrams(response, n)
        if not p_ng and not r_ng:
            return 0.5
        if not p_ng or not r_ng:
            return 0.7

        intersection = len(p_ng & r_ng)
        union = len(p_ng | r_ng)
        similarity = intersection / max(union, 1)
        return float(round(1.0 - similarity, 4))

    def _embedding_score(self, embeddings: Mapping[str, Sequence[float]]) -> float:
        if "prompt" in embeddings and "response" in embeddings:
            p = embeddings["prompt"]
            r = embeddings["response"]
            dot = sum(a * b for a, b in zip(p, r))
            mag_p = math.sqrt(sum(a * a for a in p))
            mag_r = math.sqrt(sum(b * b for b in r))
            if mag_p == 0 or mag_r == 0:
                return 0.5
            cosine = dot / (mag_p * mag_r)
            return float(round(max(0.0, 1.0 - cosine), 4))
        return 0.5


class L4MultiTokenAggregation:
    """Multi-token aggregation: rising per-token σ over a window suggests onset of drift."""

    def score(
        self,
        prompt: str,
        response: str,
        per_token_sigma: Optional[Sequence[float]] = None,
    ) -> float:
        if per_token_sigma is None:
            return self._chunk_score(prompt, response)
        pts = list(per_token_sigma)
        if len(pts) < 2:
            return float(sum(pts) / max(len(pts), 1))

        window = 5
        for i in range(len(pts) - window):
            chunk = pts[i : i + window]
            if all(chunk[j + 1] > chunk[j] for j in range(len(chunk) - 1)):
                return float(min(1.0, sum(chunk) / len(chunk) + 0.2))

        return float(sum(pts) / len(pts))

    def _chunk_score(self, prompt: str, response: str) -> float:
        words = response.split()
        if len(words) < 6:
            return 0.3

        chunk_size = max(3, len(words) // 4)
        chunks = [" ".join(words[i : i + chunk_size]) for i in range(0, len(words), chunk_size)]

        l2 = L2LogProbVariance()
        chunk_sigma = [l2._estimate_from_text(prompt, chunk) for chunk in chunks]

        if len(chunk_sigma) >= 2 and chunk_sigma[-1] > chunk_sigma[0] + 0.2:
            return float(min(1.0, chunk_sigma[-1]))

        return float(sum(chunk_sigma) / len(chunk_sigma))


class L5SpectralSignature:
    """Structural anomalies as a proxy when attention weights are unavailable."""

    def score(
        self,
        prompt: str,
        response: str,
        attention_weights: Optional[Sequence[float]] = None,
    ) -> float:
        if attention_weights is not None:
            return self._attention_score(attention_weights)
        return self._structure_score(prompt, response)

    def _structure_score(self, prompt: str, response: str) -> float:
        sigma = 0.2
        sentences = response.split(".")
        unique = {s.strip().lower() for s in sentences if s.strip()}
        if sentences and len(unique) < len(sentences) * 0.5:
            sigma += 0.3

        if "?" not in prompt and response.count("?") > 2:
            sigma += 0.15

        caps_ratio = sum(1 for c in response if c.isupper()) / max(len(response), 1)
        if caps_ratio > 0.5 and len(response) > 20:
            sigma += 0.2

        lower = response.lower()
        if " is " in lower and " is not " in lower:
            sigma += 0.15

        return float(min(1.0, sigma))

    def _attention_score(self, weights: Sequence[float]) -> float:
        w = list(weights)
        if not w:
            return 0.5
        s = sum(w)
        if s <= 0:
            return 0.5
        norm = [max(x / s, 1e-10) for x in w]
        entropy = -sum(x * math.log2(x) for x in norm)
        return float(min(1.0, entropy / 5.0))


class SigmaFusion:
    """Fuse L1–L5 into a single σ and a coarse verdict (thresholds 0.15 / 0.5)."""

    WEIGHTS: Dict[str, float] = {
        "L1": 0.15,
        "L2": 0.25,
        "L3": 0.25,
        "L4": 0.20,
        "L5": 0.15,
    }

    def __init__(self, weights: Optional[MutableMapping[str, float]] = None) -> None:
        self.l1 = L1EntropyProbe()
        self.l2 = L2LogProbVariance()
        self.l3 = L3HiddenStateDivergence()
        self.l4 = L4MultiTokenAggregation()
        self.l5 = L5SpectralSignature()
        self._weights: Dict[str, float] = dict(self.WEIGHTS)
        if weights:
            merged = dict(self.WEIGHTS)
            merged.update(weights)
            total = sum(merged.values())
            if total <= 0:
                raise ValueError("SigmaFusion weights must sum to a positive value")
            self._weights = {k: float(v) / total for k, v in merged.items()}

    def score(
        self,
        prompt: str,
        response: str,
        *,
        logprobs: Optional[Sequence[float]] = None,
        embeddings: Optional[Mapping[str, Sequence[float]]] = None,
        attention: Optional[Sequence[float]] = None,
        per_token_sigma: Optional[Sequence[float]] = None,
    ) -> Tuple[float, str, Dict[str, float]]:
        scores: Dict[str, float] = {
            "L1": float(self.l1.score(prompt, response)),
            "L2": float(self.l2.score(prompt, response, logprobs)),
            "L3": float(self.l3.score(prompt, response, embeddings)),
            "L4": float(self.l4.score(prompt, response, per_token_sigma)),
            "L5": float(self.l5.score(prompt, response, attention_weights=attention)),
        }
        w = self._weights
        sigma = sum(scores[k] * w.get(k, 0.0) for k in scores)
        sigma = round(min(1.0, max(0.0, sigma)), 4)

        if sigma < 0.15:
            verdict = "ACCEPT"
        elif sigma < 0.5:
            verdict = "RETHINK"
        else:
            verdict = "ABSTAIN"

        return sigma, verdict, scores


__all__ = [
    "L1EntropyProbe",
    "L2LogProbVariance",
    "L3HiddenStateDivergence",
    "L4MultiTokenAggregation",
    "L5SpectralSignature",
    "SigmaFusion",
]
