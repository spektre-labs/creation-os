# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-embed — lightweight embedding + σ sidecar (lab; not a trained encoder).

Vectors default to a deterministic hash of characters; optional ``embedder(text)->seq``
replaces that path. See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import math
from typing import Any, Callable, Dict, List, Optional, Sequence

__all__ = ["SigmaEmbed"]


class SigmaEmbed:
    """Build pseudo-embeddings, per-dimension σ via the gate, similarity, RAG rerank sketch."""

    def __init__(self, gate: Any = None, *, dim: int = 8) -> None:
        from cos.sigma_gate import SigmaGate

        self.gate = gate or SigmaGate()
        self.dim = max(2, int(dim))

    def _default_embed(self, text: str) -> List[float]:
        t = str(text)
        vec: List[float] = []
        for i in range(self.dim):
            acc = 0.0
            for j, c in enumerate(t):
                acc += (ord(c) * (j + 1) * (i + 3)) % 997 / 997.0
            vec.append(acc % 1.0)
        # L2 normalize
        norm = math.sqrt(sum(x * x for x in vec)) or 1.0
        return [x / norm for x in vec]

    def embed_sigma(
        self,
        text: str,
        embedder: Optional[Callable[[str], Sequence[float]]] = None,
    ) -> Dict[str, Any]:
        if embedder is not None:
            emb = [float(x) for x in embedder(str(text))]
        else:
            emb = self._default_embed(text)
        sigmas: List[float] = []
        for i, v in enumerate(emb):
            s = float(
                self.gate.compute_sigma(
                    None,
                    None,
                    f"embed_dim:{i}",
                    f"{text[:40]!s}|{v:.6f}",
                ),
            )
            sigmas.append(s)
        mean = sum(sigmas) / max(len(sigmas), 1)
        return {"embedding": emb, "sigma_dims": sigmas, "mean_sigma": mean}

    def similarity_with_sigma(
        self,
        emb_a: Sequence[float],
        emb_b: Sequence[float],
        sigma_a: Sequence[float],
        sigma_b: Sequence[float],
    ) -> Dict[str, Any]:
        """Cosine on vectors; confidence shrinks when either side has noisy σ."""
        def dot(u: Sequence[float], v: Sequence[float]) -> float:
            return sum(float(x) * float(y) for x, y in zip(u, v))

        def norm(u: Sequence[float]) -> float:
            return math.sqrt(sum(float(x) ** 2 for x in u)) or 1.0

        cos = dot(emb_a, emb_b) / (norm(emb_a) * norm(emb_b))
        sa = sum(float(x) for x in sigma_a) / max(len(sigma_a), 1)
        sb = sum(float(x) for x in sigma_b) / max(len(sigma_b), 1)
        conf = max(0.0, 1.0 - (sa + sb) / 2.0)
        return {"cosine": round(cos, 6), "sigma_confidence": round(conf, 6)}

    def cluster_sigma(
        self,
        embeddings: List[Sequence[float]],
        *,
        k: int = 2,
    ) -> Dict[str, Any]:
        """1-D k-means on vector means — toy clustering with per-cluster mean |emb| proxy σ."""
        if not embeddings:
            return {"k": k, "labels": [], "cluster_sigma": []}
        feats = [sum(float(x) for x in e) / max(len(e), 1) for e in embeddings]
        lo, hi = min(feats), max(feats)
        centers = [lo + (hi - lo) * (i + 0.5) / max(k, 1) for i in range(k)]
        labels: List[int] = []
        for f in feats:
            nearest = min(range(k), key=lambda j: abs(f - centers[j]))
            labels.append(int(nearest))
        cluster_sigma: List[float] = []
        for j in range(k):
            mem = [feats[i] for i, lab in enumerate(labels) if lab == j]
            cluster_sigma.append(sum(mem) / max(len(mem), 1) if mem else 0.0)
        return {"k": k, "labels": labels, "cluster_sigma": [round(s, 6) for s in cluster_sigma]}

    def drift_detection(
        self,
        old_embeddings: List[Sequence[float]],
        new_embeddings: List[Sequence[float]],
    ) -> Dict[str, Any]:
        """Mean L1 distance of pseudo-means; σ is that distance clamped."""
        def mean_vec(rows: List[Sequence[float]]) -> List[float]:
            if not rows:
                return [0.0] * self.dim
            width = max(len(r) for r in rows)
            acc = [0.0] * width
            for r in rows:
                for i, v in enumerate(r):
                    acc[i] += float(v)
            n = len(rows)
            return [x / n for x in acc]

        mo = mean_vec(old_embeddings)
        mn = mean_vec(new_embeddings)
        dist = sum(abs(float(a) - float(b)) for a, b in zip(mo, mn))
        dist += abs(len(mo) - len(mn)) * 0.01
        shift = max(0.0, min(1.0, dist))
        return {"shift_sigma": round(shift, 6), "l1_mean": round(dist, 6)}

    def rag_rerank_chunks(
        self,
        prompt: str,
        chunks: List[str],
    ) -> List[Dict[str, Any]]:
        """Score each chunk with the gate; lower σ first."""
        scored: List[Dict[str, Any]] = []
        for i, ch in enumerate(chunks):
            s = float(self.gate.compute_sigma(None, None, str(prompt), str(ch)))
            scored.append({"index": i, "chunk": ch, "sigma": round(s, 6)})
        scored.sort(key=lambda x: x["sigma"])
        return scored
