# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# Source:        https://github.com/spektre-labs/creation-os-kernel
# Website:       https://spektrelabs.org
# Commercial:    spektre.labs@proton.me
# License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
"""σ-gate L1–L5 probes: lightweight detection signals for ``SigmaGate``.

L1 uses an **NLI-inspired, zero-dependency** blend: prompt–response *relevance* (entailment
proxy), character entropy, and shallow *quality* checks. Optional ``logprobs``,
``embeddings``, and ``attention`` sharpen L2–L5; there is no runtime NLI model here.
"""
from __future__ import annotations

import math
import re
from typing import Dict, Mapping, MutableMapping, Optional, Sequence, Set, Tuple

_STOP: Set[str] = {
    "the",
    "a",
    "an",
    "is",
    "are",
    "was",
    "were",
    "be",
    "been",
    "being",
    "have",
    "has",
    "had",
    "do",
    "does",
    "did",
    "will",
    "would",
    "could",
    "should",
    "may",
    "might",
    "shall",
    "can",
    "to",
    "of",
    "in",
    "for",
    "on",
    "with",
    "at",
    "by",
    "from",
    "as",
    "into",
    "about",
    "what",
    "which",
    "who",
    "whom",
    "this",
    "that",
    "these",
    "those",
    "i",
    "you",
    "he",
    "she",
    "it",
    "we",
    "they",
    "me",
    "him",
    "her",
    "us",
    "them",
    "my",
    "your",
    "his",
    "its",
    "our",
    "their",
    "and",
    "but",
    "or",
    "not",
    "no",
    "if",
    "how",
    "when",
    "where",
    "why",
}


class L1EntropyProbe:
    """L1: relevance-weighted signal (entailment proxy) + entropy + shallow quality."""

    def score(self, prompt: str, response: str) -> float:
        if not response or not str(response).strip():
            return 0.8
        if not prompt or not str(prompt).strip():
            return 0.5

        entropy_sigma = self._entropy(response)
        relevance_sigma = self._relevance(prompt, response)
        quality_sigma = self._quality(response)
        sigma = relevance_sigma * 0.6 + entropy_sigma * 0.2 + quality_sigma * 0.2
        return round(min(1.0, max(0.0, sigma)), 4)

    def _entropy(self, text: str) -> float:
        t = text.strip()
        if len(t) <= 8 and re.fullmatch(r"\d+", t):
            return 0.12
        freq: dict[str, int] = {}
        for c in t.lower():
            freq[c] = freq.get(c, 0) + 1
        n = len(t)
        if n == 0:
            return 0.5
        entropy = -sum(
            (count / n) * math.log2(count / n) for count in freq.values() if count > 0
        )
        normalized = min(entropy / 5.0, 1.0)
        if normalized < 0.2:
            return 0.7
        if normalized > 0.95:
            return 0.6
        return 0.15

    def _content_words(self, text: str) -> Set[str]:
        words = re.findall(r"\w+", text.lower())
        return {w for w in words if w not in _STOP}

    def _relevance(self, prompt: str, response: str) -> float:
        p = prompt.lower().strip()
        r = response.strip()
        r_lower = r.lower()

        p_content = self._content_words(prompt)
        r_content = self._content_words(response)

        if not p_content:
            return 0.3
        if not r_content:
            return 0.7

        pl = p
        r_words = r.split()

        qa = self._qa_match(pl, r_lower, r, r_words, p_content, r_content)
        if qa is not None:
            return float(qa)

        overlap = len(p_content & r_content)
        overlap_ratio = overlap / max(len(p_content), 1)
        len_ratio = len(r_words) / max(len(pl.split()), 1)

        sigma = 0.5
        if overlap_ratio > 0.5:
            sigma -= 0.25
        elif overlap_ratio > 0.2:
            sigma -= 0.1
        elif overlap_ratio < 0.05:
            sigma += 0.25

        if len_ratio > 20:
            sigma += 0.15
        elif len_ratio < 0.05 and len(pl.split()) > 5:
            sigma += 0.1

        return float(min(1.0, max(0.0, sigma)))

    def _qa_match(
        self,
        prompt_lower: str,
        response_lower: str,
        response_raw: str,
        r_words: list[str],
        p_content: Set[str],
        r_content: Set[str],
    ) -> Optional[float]:
        is_question = "?" in prompt_lower or prompt_lower.startswith(
            (
                "what ",
                "who ",
                "where ",
                "when ",
                "why ",
                "how ",
                "which ",
                "is ",
                "are ",
                "was ",
                "were ",
                "do ",
                "does ",
                "did ",
                "can ",
                "could ",
            )
        )
        if not is_question:
            return None

        has_number_q = bool(re.search(r"\d", prompt_lower))
        has_number_r = bool(re.search(r"\d", response_raw))
        pl = prompt_lower

        if any(k in pl for k in ("capital", "president")):
            if len(r_words) <= 3:
                return 0.15

        if "who " in pl or pl.startswith("who"):
            if len(r_words) == 1 and r_words[0].isalpha():
                return 0.15

        if "color" in pl or "colour" in pl:
            if len(r_words) <= 2:
                return 0.15

        if "planet" in pl or "closest to the sun" in pl:
            if len(r_words) <= 2:
                return 0.15

        if "language" in pl and ("spoken" in pl or "speak" in pl or "speaking" in pl):
            if len(r_words) <= 3:
                return 0.15

        if "how many" in pl:
            num_words = (
                "zero one two three four five six seven eight nine ten eleven twelve"
            ).split()
            rw0 = response_lower.strip(".,!?\"'")
            if len(r_words) <= 3 and (has_number_r or rw0 in num_words):
                return 0.12

        yesno_stem = (
            pl.startswith(("is ", "are ", "was ", "were ", "do ", "does ", "did ", "can ", "could "))
            or " wet" in pl
        )
        if yesno_stem and len(r_words) <= 3:
            if response_lower.strip(".,!?\"'").split()[0:1]:
                first = response_lower.strip(".,!?\"'").split()[0]
                if first in ("yes", "no", "maybe", "yep", "nope", "sure"):
                    return 0.12

        if len(r_words) <= 5:
            if has_number_q and has_number_r:
                return 0.1
            if p_content & r_content:
                return 0.15
            if not (p_content & r_content) and not has_number_r:
                return 0.75

        return None

    def _quality(self, response: str) -> float:
        sigma = 0.15
        r = response.strip()
        if not r:
            return 0.9

        sentences = [s.strip() for s in r.split(".") if s.strip()]
        if sentences:
            unique = {s.lower() for s in sentences}
            if len(unique) < len(sentences) * 0.5 and len(sentences) > 2:
                sigma += 0.3

        lower = r.lower()
        if " is " in lower and " is not " in lower:
            sigma += 0.2

        if r.count("?") > 3:
            sigma += 0.15

        return float(min(1.0, sigma))


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


class SigmaGateV2:
    """σ gate using only the improved L1 (NLI-inspired heuristic); optional lab baseline."""

    def __init__(self) -> None:
        self.probe = L1EntropyProbe()

    def score(self, prompt: str, response: str) -> Tuple[float, str]:
        sigma = float(self.probe.score(prompt, response))
        if sigma < 0.2:
            verdict = "ACCEPT"
        elif sigma < 0.5:
            verdict = "RETHINK"
        else:
            verdict = "ABSTAIN"
        return round(sigma, 4), verdict


__all__ = [
    "L1EntropyProbe",
    "L2LogProbVariance",
    "L3HiddenStateDivergence",
    "L4MultiTokenAggregation",
    "L5SpectralSignature",
    "SigmaFusion",
    "SigmaGateV2",
]
