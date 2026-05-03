# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-index — in-memory corpus index with per-doc / per-chunk σ (lab RAG backend).

Pairs with :mod:`cos.rag` conceptually; no vector DB required. See
``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import hashlib
import time
from typing import Any, Dict, List, Optional, Sequence

__all__ = ["SigmaIndex"]


class SigmaIndex:
    """Chunk documents, score with ``SigmaGate``, search by σ-ranked overlap."""

    def __init__(self, gate: Any = None, *, stale_sigma_bump_per_day: float = 0.01) -> None:
        from cos.sigma_gate import SigmaGate

        self.gate = gate or SigmaGate()
        self.stale_sigma_bump_per_day = float(stale_sigma_bump_per_day)
        self._docs: List[Dict[str, Any]] = []

    def index(self, documents: Sequence[str]) -> Dict[str, Any]:
        self._docs.clear()
        for d in documents:
            self.incremental_add(str(d))
        return {"indexed": len(self._docs)}

    def incremental_add(self, doc: str) -> Dict[str, Any]:
        chunks = [c.strip() for c in str(doc).split("\n\n") if c.strip()]
        if not chunks:
            chunks = [str(doc).strip() or "empty"]
        doc_sigma = float(self.gate.compute_sigma(None, None, "doc", doc[:4000]))
        chunk_rows: List[Dict[str, Any]] = []
        for ch in chunks:
            s = float(self.gate.compute_sigma(None, None, "chunk", ch[:2000]))
            emb = self._pseudo_emb(ch)
            chunk_rows.append(
                {
                    "text": ch,
                    "sigma": s,
                    "embedding": emb,
                },
            )
        rec = {
            "id": hashlib.sha256(doc[:500].encode()).hexdigest()[:12],
            "text": doc,
            "doc_sigma": doc_sigma,
            "chunks": chunk_rows,
            "indexed_at": time.time(),
        }
        self._docs.append(rec)
        return rec

    def search(self, query: str, *, top_k: int = 5) -> List[Dict[str, Any]]:
        hits: List[Dict[str, Any]] = []
        q = str(query)
        for doc in self._docs:
            for i, ch in enumerate(doc["chunks"]):
                s_gate = float(self.gate.compute_sigma(None, None, q, ch["text"]))
                sim = self._sim(q, ch["text"])
                adj = self._stale_adjust(doc, s_gate)
                hits.append(
                    {
                        "doc_id": doc["id"],
                        "chunk_ix": i,
                        "text": ch["text"],
                        "sigma": round(adj, 6),
                        "sim": round(sim, 6),
                    },
                )
        hits.sort(key=lambda h: (h["sigma"], -h["sim"]))
        return hits[: max(1, int(top_k))]

    def sigma_quality_score(self, document: str) -> float:
        return float(self.gate.compute_sigma(None, None, "quality", str(document)[:4000]))

    def stale_detection(self, doc_id: str, *, now: Optional[float] = None) -> Optional[Dict[str, Any]]:
        t = float(now if now is not None else time.time())
        for doc in self._docs:
            if doc["id"] != doc_id:
                continue
            age_days = max(0.0, (t - float(doc["indexed_at"])) / 86400.0)
            bump = age_days * self.stale_sigma_bump_per_day
            return {"stale_sigma_bump": round(bump, 6), "age_days": round(age_days, 4)}
        return None

    def dedup_candidates(self, threshold: float = 0.92) -> List[Dict[str, Any]]:
        dups: List[Dict[str, Any]] = []
        for i, a in enumerate(self._docs):
            for j, b in enumerate(self._docs):
                if j <= i:
                    continue
                sim = self._emb_cos(a["chunks"][0]["embedding"], b["chunks"][0]["embedding"])
                if sim >= threshold:
                    dups.append({"i": i, "j": j, "similarity": round(sim, 6)})
        return dups

    def _stale_adjust(self, doc: Dict[str, Any], sigma: float) -> float:
        st = self.stale_detection(doc["id"])
        if st is None:
            return sigma
        return min(1.0, float(sigma) + float(st["stale_sigma_bump"]))

    def _pseudo_emb(self, text: str) -> List[float]:
        dim = 8
        v = [0.0] * dim
        for i, c in enumerate(str(text).lower()[:200]):
            v[i % dim] += (ord(c) % 17) / 17.0
        n = sum(x * x for x in v) ** 0.5 or 1.0
        return [x / n for x in v]

    def _sim(self, query: str, chunk: str) -> float:
        a = set(str(query).lower().split())
        b = set(str(chunk).lower().split())
        if not a:
            return 0.0
        return len(a & b) / len(a)

    def _emb_cos(self, u: Sequence[float], v: Sequence[float]) -> float:
        return float(sum(float(x) * float(y) for x, y in zip(u, v)))
