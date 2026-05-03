# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-RAG — dependency-light retrieval lab with σ as chunk filter (no cross-encoder).

Hybrid and semantic-chunk paths are **toy** stand-ins for BM25/vector/RRF; use real indices
in production. See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import math
import re
from typing import Any, Callable, Dict, List, Optional, Sequence

__all__ = ["SigmaRAG"]


class SigmaRAG:
    """Retrieve → σ-rerank → augment → (optional) generate with gate verification."""

    def __init__(
        self,
        gate: Any = None,
        *,
        sigma_keep_below: float = 0.55,
        rrf_k: int = 60,
    ) -> None:
        from cos.sigma_gate import SigmaGate

        self.gate = gate or SigmaGate()
        self.sigma_keep_below = float(sigma_keep_below)
        self.rrf_k = int(rrf_k)
        self._corpus: List[str] = []

    def index_documents(self, chunks: Sequence[str]) -> None:
        self._corpus = [str(c) for c in chunks]

    def retrieve(
        self,
        query: str,
        top_k: int,
        corpus: Optional[Sequence[str]] = None,
    ) -> List[Dict[str, Any]]:
        corp = [str(c) for c in (corpus if corpus is not None else self._corpus)]
        ranked: List[tuple[float, int, str]] = []
        for i, ch in enumerate(corp):
            r = self._bm25_like(query, ch) + self._vec_overlap(query, ch)
            ranked.append((r, i, ch))
        ranked.sort(key=lambda x: -x[0])
        out: List[Dict[str, Any]] = []
        for r, i, ch in ranked[: max(1, int(top_k))]:
            sigma = float(self.gate.compute_sigma(None, None, str(query), ch))
            out.append({"chunk": ch, "sigma": sigma, "retrieval_score": round(r, 6), "id": i})
        return out

    def sigma_rerank(
        self,
        query: str,
        chunks: Sequence[str],
        gate: Any,
    ) -> Dict[str, Any]:
        rows: List[Dict[str, Any]] = []
        for ch in chunks:
            sigma = float(gate.compute_sigma(None, None, str(query), str(ch)))
            rows.append({"chunk": str(ch), "sigma": sigma})
        rows.sort(key=lambda x: x["sigma"])
        kept = [r for r in rows if r["sigma"] <= self.sigma_keep_below]
        return {"sorted": rows, "kept": kept}

    def hybrid_search(
        self,
        query: str,
        corpus: Optional[Sequence[str]] = None,
        *,
        top_n: int = 8,
    ) -> Dict[str, Any]:
        corp = [str(c) for c in (corpus if corpus is not None else self._corpus)]
        bm25_ids = self._rank_ids(query, corp, key=self._bm25_like)
        vec_ids = self._rank_ids(query, corp, key=self._vec_overlap)
        rrf: Dict[int, float] = {}
        k = self.rrf_k
        for rank, idx in enumerate(bm25_ids[:top_n]):
            rrf[idx] = rrf.get(idx, 0.0) + 1.0 / (k + rank + 1)
        for rank, idx in enumerate(vec_ids[:top_n]):
            rrf[idx] = rrf.get(idx, 0.0) + 1.0 / (k + rank + 1)
        ordered = sorted(rrf.keys(), key=lambda i: -rrf[i])
        return {"ids": ordered, "rrf_scores": {i: round(rrf[i], 6) for i in ordered}}

    def semantic_chunk(
        self,
        document: str,
        *,
        cosine_threshold: float = 0.82,
    ) -> List[str]:
        t = str(document).strip()
        if not t:
            return []
        sents = re.split(r"(?<=[.!?])\s+", t)
        chunks: List[str] = []
        buf: List[str] = []
        prev: Optional[List[float]] = None
        for sent in sents:
            if not sent.strip():
                continue
            v = self._sent_vec(sent)
            if not buf:
                buf.append(sent)
                prev = v
                continue
            sim = self._cos(prev or v, v)
            if sim >= float(cosine_threshold):
                buf.append(sent)
                prev = [0.5 * (a + b) for a, b in zip(prev or v, v)]
            else:
                chunks.append(" ".join(buf))
                buf = [sent]
                prev = v
        if buf:
            chunks.append(" ".join(buf))
        return chunks

    def augment(self, query: str, sigma_filtered_chunks: Sequence[Any]) -> str:
        parts: List[str] = []
        for item in sigma_filtered_chunks:
            if isinstance(item, dict):
                parts.append(str(item.get("chunk", "")))
            else:
                parts.append(str(item))
        ctx = "\n---\n".join(p for p in parts if p)
        return f"Context:\n{ctx}\n\nQuestion: {query}"

    def generate(
        self,
        augmented_prompt: str,
        model: Any,
        gate: Any,
    ) -> Dict[str, Any]:
        if hasattr(model, "generate"):
            text = str(model.generate(augmented_prompt))
        else:
            text = str(model)
        sigma, verdict = gate.score(str(augmented_prompt), text)
        return {
            "text": text,
            "sigma": float(sigma),
            "verdict": str(verdict),
        }

    def pipeline(
        self,
        query: str,
        corpus: Sequence[str],
        model: Any,
    ) -> Dict[str, Any]:
        raw = self.retrieve(query, 8, corpus)
        chunks = [x["chunk"] for x in raw]
        rr = self.sigma_rerank(query, chunks, self.gate)
        aug = self.augment(query, rr["kept"])
        gen = self.generate(aug, model, self.gate)
        ver_sigma = float(
            self.gate.compute_sigma(None, None, str(query), gen["text"]),
        )
        gen["verify_sigma"] = ver_sigma
        return {"augmented": aug, "retrieval": raw, "rerank": rr, "generate": gen}

    def _bm25_like(self, query: str, doc: str) -> float:
        q_terms = set(str(query).lower().split())
        d_terms = str(doc).lower().split()
        if not q_terms:
            return 0.0
        overlap = sum(1 for t in q_terms if t in d_terms)
        return overlap / len(q_terms)

    def _vec_overlap(self, query: str, doc: str) -> float:
        a = set(str(query).lower())
        b = set(str(doc).lower())
        if not a:
            return 0.0
        return len(a & b) / len(a)

    def _rank_ids(
        self,
        query: str,
        corp: List[str],
        *,
        key: Callable[[str, str], float],
    ) -> List[int]:
        scored = [(key(query, c), i) for i, c in enumerate(corp)]
        scored.sort(key=lambda x: -x[0])
        return [i for _r, i in scored]

    def _sent_vec(self, sent: str) -> List[float]:
        dim = 16
        v = [0.0] * dim
        for i, c in enumerate(str(sent).lower()[:128]):
            v[i % dim] += (ord(c) % 13) / 13.0
        n = math.sqrt(sum(x * x for x in v)) or 1.0
        return [x / n for x in v]

    def _cos(self, u: Sequence[float], v: Sequence[float]) -> float:
        return float(sum(float(a) * float(b) for a, b in zip(u, v)))
