# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tokenizer-driven σ_input lab — rare / fragmented / OOV tokens inflate uncertainty.

Feeds concepts into :class:`cos.pipeline.Pipeline` / :class:`cos.stream.SigmaStream` as an
optional **input stress** scalar. Not a substitute for a production tokenizer audit.
"""
from __future__ import annotations

from cos.sigma_gate import SigmaGate  # noqa: F401 — σ kernel anchor (sigma_gate.h / Python lite)

import math
import re
import unicodedata
from typing import Any, Dict, List, Optional, Protocol, Sequence, Tuple

__all__ = ["SigmaTokenizer"]


class _SupportsEncode(Protocol):
    def encode(self, text: str, **kwargs: Any) -> List[int]: ...

    def decode(self, ids: List[int], **kwargs: Any) -> str: ...

    @property
    def vocab_size(self) -> int: ...


class SigmaTokenizer:
    """Analyze token IDs for fragmentation and rarity; estimate σ_input contribution."""

    def __init__(self, *, unk_id: int = -1) -> None:
        self.unk_id = int(unk_id)

    def analyze(self, text: str, tokenizer: Any) -> Dict[str, Any]:
        enc = getattr(tokenizer, "encode", None)
        if not callable(enc):
            raise TypeError("tokenizer must provide encode(text) -> List[int]")
        ids: List[int] = list(enc(str(text)))
        vocab = int(getattr(tokenizer, "vocab_size", 32000) or 32000)
        oov = any(tid == self.unk_id or tid < 0 or tid >= vocab for tid in ids)
        rare = 0
        hi = int(vocab * 0.92)
        for tid in ids:
            if tid >= hi:
                rare += 1
        frag = self.fragmentation_score(text, tokenizer)
        sigma_tok = self.sigma_input_estimate(ids, fragmentation_hint=frag, oov=oov, rare_frac=rare / max(len(ids), 1))
        return {
            "token_ids": ids,
            "n_tokens": len(ids),
            "oov_hit": bool(oov),
            "rare_count": rare,
            "fragmentation_score": frag,
            "sigma_input": sigma_tok,
            "per_token_sigma_hint": [
                1.0 if (tid == self.unk_id or tid < 0 or tid >= vocab) else min(1.0, (tid / max(vocab, 1)) ** 0.5)
                for tid in ids
            ],
        }

    def fragmentation_score(self, text: str, tokenizer: Any) -> float:
        """Ratio of tokens to whitespace words; >0 means extra splitting."""
        words = [w for w in re.split(r"\s+", str(text).strip()) if w]
        n_words = max(len(words), 1)
        enc = getattr(tokenizer, "encode", None)
        if not callable(enc):
            raise TypeError("tokenizer must provide encode")
        n_tok = max(len(enc(str(text))), 1)
        raw = n_tok / float(n_words) - 1.0
        return float(max(0.0, min(2.0, raw)))

    def sigma_input_estimate(
        self,
        tokens: Sequence[int],
        *,
        fragmentation_hint: Optional[float] = None,
        oov: bool = False,
        rare_frac: Optional[float] = None,
    ) -> float:
        """Map tokenizer pathology buckets into [0,1] σ_input (lab heuristic)."""
        n = max(len(tokens), 1)
        rare = float(rare_frac) if rare_frac is not None else 0.0
        frag = float(fragmentation_hint) if fragmentation_hint is not None else 0.0
        base = 0.18 * rare + 0.22 * min(1.0, frag) + (0.6 if oov else 0.0)
        length_pen = min(0.15, math.log1p(n) / 25.0)
        return float(max(0.0, min(1.0, base + length_pen)))

    def compare_tokenizers(self, text: str, tokenizers: Sequence[Any]) -> Dict[str, Any]:
        """Return which tokenizer yields lower σ_input (prefer lower fragmentation / OOV)."""
        rows = []
        best: Tuple[float, int] = (1.0, -1)
        for i, tok in enumerate(tokenizers):
            a = self.analyze(text, tok)
            s = float(a["sigma_input"])
            rows.append({"index": i, "sigma_input": s, "n_tokens": a["n_tokens"], "oov": a["oov_hit"]})
            if s < best[0]:
                best = (s, i)
        return {"best_index": best[1], "best_sigma_input": best[0], "rows": rows}

    def detect_encoding_artifacts(self, tokens: Sequence[Any]) -> Dict[str, Any]:
        """Flag decode / Unicode replacement issues (post-decode string checks)."""
        issues: List[str] = []
        for t in tokens:
            s = str(t)
            if "\ufffd" in s or "�" in s:
                issues.append("replacement_char")
            norm = unicodedata.normalize("NFKC", s)
            if norm != s:
                issues.append("nfkc_delta")
        return {"issues": issues, "suspicious": len(issues) > 0}

