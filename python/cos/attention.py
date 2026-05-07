# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Attention read through a σ lens (lab scaffolding).

Corpus framing (operational, not a claim about a specific transformer): **Q** is the
observer’s query (declared interest), **K** is what is available to attend to, **V** is
what would be read out (meaning / delivered content; optional in :meth:`attention_σ`),
and a **sigmoid** on the scaled dot score acts as a concentration filter analogous to
how mass concentrates after a softmax family map — **high Q·K** (scaled) ⇒ more mass on
the match ⇒ **lower σ** (1=1), **low Q·K** ⇒ **higher σ** (1≠1).

**NOT AGI ACHIEVED** — analytic diagnostics only; see ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import math
from typing import Any, Dict, List, Sequence, Union

__all__ = ["AttentionAnalyzer"]

from cos.sigma_gate import SigmaGate

try:
    import numpy as np

    _HAS_NP = True
except ImportError:  # pragma: no cover
    np = None  # type: ignore[misc, assignment]
    _HAS_NP = False


Number = Union[float, int]


class AttentionAnalyzer:
    """Score attention-style Q/K interaction and multi-head coherence (optional NumPy)."""

    def __init__(self, gate: Any = None) -> None:
        self.gate = gate or SigmaGate()

    @staticmethod
    def _align_vectors(Q: Sequence[Number], K: Sequence[Number]) -> tuple[List[float], List[float]]:
        q = [float(x) for x in Q]
        k = [float(x) for x in K]
        n = min(len(q), len(k))
        if n == 0:
            return [], []
        return q[:n], k[:n]

    @staticmethod
    def _sigmoid(x: float) -> float:
        x = max(-50.0, min(50.0, float(x)))
        return 1.0 / (1.0 + math.exp(-x))

    def attention_σ(
        self,
        Q: Sequence[Number],
        K: Sequence[Number],
        V: Any = None,
    ) -> Dict[str, Any]:
        """Scaled dot Q·K / sqrt(d) → sigmoid mass; σ = 1 − mass (high match ⇒ low σ)."""
        del V  # Reserved: delivered values / downstream readout (callers may use elsewhere).
        qv, kv = self._align_vectors(Q, K)
        if not qv:
            return {
                "score": 0.0,
                "attention": 0.5,
                "σ": 0.5,
                "coherent": False,
                "interpretation": "Q mismatches K: observer ≠ observed (1≠1)",
            }

        if not _HAS_NP:
            return self._attention_σ_fallback(qv, kv)

        q = np.asarray(qv, dtype=np.float32)
        k = np.asarray(kv, dtype=np.float32)
        score = float(np.dot(q, k)) / math.sqrt(len(q))
        attention = self._sigmoid(score)
        σ = 1.0 - attention
        return self._pack_attention_out(score, attention, σ)

    def _attention_σ_fallback(self, qv: Sequence[float], kv: Sequence[float]) -> Dict[str, Any]:
        """Pure-Python path (aligned float vectors, same length)."""
        score = sum(float(a) * float(b) for a, b in zip(qv, kv, strict=True)) / math.sqrt(len(qv))
        attention = self._sigmoid(score)
        σ = 1.0 - attention
        return self._pack_attention_out(score, attention, σ)

    def _pack_attention_out(self, score: float, attention: float, σ: float) -> Dict[str, Any]:
        σ = float(max(0.0, min(1.0, σ)))
        if σ < 0.3:
            interp = "Q found K: declared interest = available info (1=1)"
        elif σ < 0.7:
            interp = "Q partially matches K: some distortion"
        else:
            interp = "Q mismatches K: observer ≠ observed (1≠1)"
        return {
            "score": round(float(score), 4),
            "attention": round(float(attention), 4),
            "σ": round(float(σ), 4),
            "coherent": σ < 0.3,
            "interpretation": interp,
        }

    def sparsity_σ(self, attention_matrix: Any) -> Dict[str, Any]:
        """Gini on flattened mass; high Gini (peaked) ⇒ low σ; uniform ⇒ high σ."""
        if not _HAS_NP:
            return {"sparsity": 0.5, "σ": 0.5, "focused": False}

        mat = np.asarray(attention_matrix, dtype=np.float32)
        flat = np.sort(mat.flatten())
        n = int(flat.size)
        if n == 0 or float(flat.sum()) == 0.0:
            return {"sparsity": 1.0, "σ": 1.0, "focused": True}

        s = float(flat.sum())
        index = np.arange(1, n + 1, dtype=np.float64)
        gini = float((2.0 * np.sum(index * flat.astype(np.float64)) / (n * s)) - (n + 1) / n)
        gini = max(0.0, min(1.0, gini))
        σ = 1.0 - gini
        return {
            "sparsity": round(gini, 4),
            "σ": round(float(σ), 4),
            "focused": gini > 0.5,
        }

    def head_coherence(self, head_outputs: Sequence[Any]) -> Dict[str, Any]:
        """Pairwise cosine similarity across heads; disagreement ⇒ higher σ."""
        if len(head_outputs) < 2:
            return {"coherence": 0.5, "σ": 0.5, "n_heads": len(head_outputs)}

        if not _HAS_NP:
            return {"coherence": 0.5, "σ": 0.5, "n_heads": len(head_outputs)}

        outputs = [np.asarray(h, dtype=np.float32).reshape(-1) for h in head_outputs]
        sims: List[float] = []
        for i in range(len(outputs)):
            for j in range(i + 1, len(outputs)):
                dot = float(np.dot(outputs[i], outputs[j]))
                norm = float(np.linalg.norm(outputs[i]) * np.linalg.norm(outputs[j]))
                if norm > 0.0:
                    sims.append(dot / norm)

        if not sims:
            return {"coherence": 0.5, "σ": 0.5, "n_heads": len(head_outputs)}

        avg_sim = float(sum(sims) / len(sims))
        σ = max(0.0, min(1.0, (1.0 - avg_sim) / 2.0))
        return {
            "head_agreement": round(avg_sim, 4),
            "coherence": round(avg_sim, 4),
            "σ": round(float(σ), 4),
            "n_heads": len(head_outputs),
            "coherent": σ < 0.3,
        }
