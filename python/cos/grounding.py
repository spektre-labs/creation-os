# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Factual grounding lab — context attention mass, FFN-norm proxy, span overlap (no retriever).

For production RAG, wire real chunks and logits. See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import re
from typing import Any, Dict, List, Optional, Sequence, Tuple

__all__ = ["SigmaGrounding"]


class SigmaGrounding:
    """Heuristic grounding checks layered on :class:`cos.sigma_gate.SigmaGate` when needed."""

    def __init__(self, gate: Any = None) -> None:
        from cos.sigma_gate import SigmaGate

        self.gate = gate or SigmaGate()

    def ground_check(self, prompt: str, response: str, context: str) -> Dict[str, Any]:
        packed = f"CONTEXT:\n{context}\n\nPROMPT:\n{prompt}\n\nRESPONSE:\n{response}"
        sigma, verdict = self.gate.score("grounding_check", packed)
        return {"sigma": float(sigma), "verdict": verdict}

    def attention_to_context_ratio(
        self,
        attention_maps: Any,
        *,
        context_len: int,
        seq_len: Optional[int] = None,
    ) -> float:
        """Average per-row mass on context positions ``[0, context_len)``."""
        from cos.sink_probe import SigmaSinkProbe

        layers = SigmaSinkProbe._normalize_attention_maps(attention_maps)
        if not layers or context_len <= 0:
            return 0.0
        if seq_len is None:
            seq_len = len(layers[0][0])
        ctx_end = min(context_len, seq_len)
        fracs: List[float] = []
        for layer in layers:
            for head in layer:
                for i in range(len(head)):
                    row = head[i]
                    sctx = sum(float(row[j]) for j in range(ctx_end))
                    fracs.append(float(sctx))
        if not fracs:
            return 0.0
        return float(min(1.0, max(0.0, sum(fracs) / len(fracs))))

    @staticmethod
    def ffn_activation_score(ffn_outputs: Sequence[float]) -> float:
        """Mean absolute activation magnitude → proxy parametric memory pressure."""
        if not ffn_outputs:
            return 0.0
        xs = [abs(float(x)) for x in ffn_outputs]
        return float(min(1.0, sum(xs) / len(xs)))

    def source_attribution(self, response: str, context_chunks: Sequence[str]) -> Dict[str, Any]:
        """Greedy word-overlap score per chunk (lab)."""
        rw = set(re.findall(r"\w+", response.lower()))
        scores: List[Tuple[int, float]] = []
        for i, ch in enumerate(context_chunks):
            cw = set(re.findall(r"\w+", ch.lower()))
            inter = len(rw & cw)
            union = len(rw | cw) or 1
            scores.append((i, inter / union))
        best = max(scores, key=lambda x: x[1]) if scores else (-1, 0.0)
        return {"best_chunk": best[0], "score": round(best[1], 6), "all": scores}

    def ungrounded_spans(self, response: str, context: str, *, min_words: int = 2) -> Dict[str, Any]:
        """Mark sentence fragments with no token overlap with context."""
        ctx = set(re.findall(r"\w+", context.lower()))
        parts = re.split(r"(?<=[.!?])\s+", str(response).strip())
        bad: List[str] = []
        for p in parts:
            words = re.findall(r"\w+", p.lower())
            if len(words) < min_words:
                continue
            if not (set(words) & ctx):
                bad.append(p.strip())
        return {"ungrounded": bad, "count": len(bad)}
