# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# Source:        https://github.com/spektre-labs/creation-os-kernel
# Website:       https://spektrelabs.org
# Commercial:    spektre.labs@proton.me
# License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
"""σ-gate L1–L5 probes: lightweight detection signals for ``SigmaGate``.

L1 uses an **NLI-inspired, zero-dependency** blend: prompt–response *relevance* (entailment
proxy plus **BM25-style** term weighting on content tokens), character entropy, and shallow
*quality* checks. Optional ``logprobs``,
``embeddings``, and ``attention`` sharpen L2–L5; there is no runtime NLI model here.
L3’s default path uses **SIF-weighted hash pseudo-embeddings** (no GloVe, no torch).
"""
from __future__ import annotations

import hashlib
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

# Single-response BM25 uses a fixed length prior so ``b`` can normalize doc length.
_REF_AVG_RESP_TOKENS: float = 24.0


class L1EntropyProbe:
    """L1: BM25-style + heuristic relevance (QA first), entropy, and shallow quality."""

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

    def _bm25_relevance(self, prompt: str, response: str, k1: float = 1.5, b: float = 0.75) -> float:
        """BM25-like term saturation on prompt content tokens (no corpus IDF; zero deps)."""
        p_toks = re.findall(r"\w+", prompt.lower())
        r_toks = re.findall(r"\w+", response.lower())
        if not p_toks or not r_toks:
            return 0.7

        p_unique = {w for w in p_toks if w not in _STOP}
        if not p_unique:
            return 0.7

        r_freq: dict[str, int] = {}
        for w in r_toks:
            r_freq[w] = r_freq.get(w, 0) + 1

        doc_len = float(len(r_toks))
        accum = 0.0
        for term in p_unique:
            if term not in r_freq:
                continue
            tf = float(r_freq[term])
            length_factor = 1.0 - b + b * (doc_len / _REF_AVG_RESP_TOKENS)
            tf_sat = (tf * (k1 + 1.0)) / (tf + k1 * length_factor)
            accum += tf_sat

        max_score = len(p_unique) * (k1 + 1.0)
        normalized = accum / max(max_score, 1.0)
        sigma = 1.0 - min(1.0, normalized * 2.0)
        return float(max(0.05, sigma))

    def _heuristic_relevance(
        self,
        pl: str,
        r_words: list[str],
        p_content: Set[str],
        r_content: Set[str],
    ) -> float:
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

        bm25_sigma = self._bm25_relevance(prompt, response)
        heuristic_sigma = self._heuristic_relevance(pl, r_words, p_content, r_content)
        return float(bm25_sigma * 0.6 + heuristic_sigma * 0.4)

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


class HashEmbedding:
    """Deterministic hash → vector pseudo-embeddings with SIF-style rare-word up-weighting."""

    DIM: int = 32
    SIF_A: float = 0.001
    FREQ: Dict[str, float] = {
        "the": 0.07,
        "be": 0.04,
        "to": 0.03,
        "of": 0.03,
        "and": 0.03,
        "a": 0.02,
        "in": 0.02,
        "that": 0.01,
        "have": 0.01,
        "i": 0.01,
        "it": 0.01,
        "for": 0.01,
        "not": 0.01,
        "on": 0.01,
        "with": 0.009,
        "he": 0.009,
        "as": 0.008,
        "you": 0.008,
        "do": 0.007,
        "at": 0.007,
        "this": 0.006,
        "but": 0.006,
        "his": 0.005,
        "by": 0.005,
        "from": 0.005,
        "they": 0.004,
        "we": 0.004,
        "say": 0.004,
        "her": 0.003,
        "she": 0.003,
        "or": 0.003,
        "an": 0.003,
        "will": 0.003,
        "my": 0.003,
        "all": 0.003,
        "would": 0.002,
        "there": 0.002,
        "their": 0.002,
        "what": 0.002,
        "so": 0.002,
        "up": 0.002,
        "out": 0.002,
        "if": 0.002,
        "about": 0.002,
        "who": 0.002,
        "get": 0.002,
        "which": 0.002,
        "go": 0.002,
        "when": 0.002,
        "can": 0.001,
        "no": 0.001,
        "is": 0.02,
        "are": 0.01,
        "was": 0.01,
        "were": 0.005,
    }

    @classmethod
    def word_to_vec(cls, word: str) -> list[float]:
        """Map a word to a pseudo-vector via SHA-256 (stable across runs and machines)."""
        w = word.lower()
        vec: list[float] = [0.0] * cls.DIM
        for i in range(cls.DIM):
            h = hashlib.sha256(f"{w}\0{i}".encode("utf-8")).digest()
            x = int.from_bytes(h[:2], "little", signed=False)
            vec[i] = (x / 65535.0) * 2.0 - 1.0
        return vec

    @classmethod
    def sif_weight(cls, word: str) -> float:
        p = cls.FREQ.get(word.lower(), 0.0001)
        return float(cls.SIF_A / (cls.SIF_A + p))

    @classmethod
    def sentence_embedding(cls, text: str) -> list[float]:
        words = re.findall(r"\w+", text.lower())
        if not words:
            return [0.0] * cls.DIM
        vec = [0.0] * cls.DIM
        total_w = 0.0
        for word in words:
            sw = cls.sif_weight(word)
            wv = cls.word_to_vec(word)
            for j in range(cls.DIM):
                vec[j] += sw * wv[j]
            total_w += sw
        if total_w > 0:
            inv = 1.0 / total_w
            vec = [v * inv for v in vec]
        return vec

    @classmethod
    def cosine_similarity(cls, vec_a: Sequence[float], vec_b: Sequence[float]) -> float:
        dot = sum(a * b for a, b in zip(vec_a, vec_b))
        mag_a = math.sqrt(sum(a * a for a in vec_a))
        mag_b = math.sqrt(sum(b * b for b in vec_b))
        if mag_a == 0.0 or mag_b == 0.0:
            return 0.0
        return float(dot / (mag_a * mag_b))

    @classmethod
    def similarity(cls, text_a: str, text_b: str) -> float:
        return cls.cosine_similarity(cls.sentence_embedding(text_a), cls.sentence_embedding(text_b))


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
    """HIDE: real embeddings when provided; else SIF-weighted hash similarity (zero deps)."""

    def score(
        self,
        prompt: str,
        response: str,
        embeddings: Optional[Mapping[str, Sequence[float]]] = None,
    ) -> float:
        if embeddings:
            return self._embedding_score(embeddings)
        sim = HashEmbedding.similarity(prompt, response)
        raw = 1.0 - (sim + 1.0) / 2.0
        return round(max(0.05, min(1.0, raw)), 4)

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
    """Lab gate: L1 (relevance / entropy / quality) + L3 (SIF hash pseudo-embedding similarity)."""

    def __init__(self) -> None:
        self.l1 = L1EntropyProbe()
        self.l3 = L3HiddenStateDivergence()

    def score(self, prompt: str, response: str) -> Tuple[float, str]:
        s1 = float(self.l1.score(prompt, response))
        s3 = float(self.l3.score(prompt, response))
        sigma = round(min(1.0, max(0.0, s1 * 0.65 + s3 * 0.35)), 4)
        if sigma < 0.2:
            verdict = "ACCEPT"
        elif sigma < 0.5:
            verdict = "RETHINK"
        else:
            verdict = "ABSTAIN"
        return sigma, verdict


__all__ = [
    "HashEmbedding",
    "L1EntropyProbe",
    "L2LogProbVariance",
    "L3HiddenStateDivergence",
    "L4MultiTokenAggregation",
    "L5SpectralSignature",
    "SigmaFusion",
    "SigmaGateV2",
]
