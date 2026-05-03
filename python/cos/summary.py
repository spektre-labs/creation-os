# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-summary — faithfulness / coverage / hallucination proxies for summarization (lab).

N-gram overlap + entity overlap; not a substitute for human eval. See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import re
from typing import Any, Dict, List, Sequence, Set

__all__ = ["SigmaSummary"]


class SigmaSummary:
    """Score summaries against sources; flag novel entities as hallucination risk."""

    @staticmethod
    def _tokens(text: str) -> List[str]:
        return re.findall(r"[a-z0-9]+", text.lower())

    @staticmethod
    def _entities(text: str) -> Set[str]:
        caps = set(re.findall(r"\b([A-Z][a-z]+(?:\s+[A-Z][a-z]+)?)\b", text))
        return {c.lower() for c in caps}

    def score_summary(self, source: str, summary: str, gate: Any) -> Dict[str, Any]:
        faith = self._faithfulness(source, summary)
        cov = self._coverage(source, summary)
        hal = self._hallucination_sigma(source, summary)
        s_gate, v_gate = gate.score("summary_check", f"SRC:{source[:1500]}\nSUM:{summary[:1500]}")
        comb = 0.35 * faith + 0.25 * (1.0 - cov) + 0.25 * hal + 0.15 * float(s_gate)
        return {
            "sigma_faithfulness": round(float(faith), 6),
            "sigma_coverage": round(float(1.0 - cov), 6),
            "sigma_hallucination": round(float(hal), 6),
            "sigma_gate": round(float(s_gate), 6),
            "verdict_gate": str(v_gate),
            "sigma_combined": round(float(comb), 6),
        }

    def _faithfulness(self, source: str, summary: str) -> float:
        st, sm = set(self._tokens(source)), set(self._tokens(summary))
        if not sm:
            return 0.9
        overlap = len(st & sm) / max(len(sm), 1)
        return max(0.0, min(1.0, 1.0 - overlap * 0.8))

    def _coverage(self, source: str, summary: str) -> float:
        key = [w for w in self._tokens(source) if len(w) > 5][:40]
        if not key:
            return 0.5
        hit = sum(1 for w in key if w in summary.lower())
        return hit / max(len(key), 1)

    def _hallucination_sigma(self, source: str, summary: str) -> float:
        es, sm = self._entities(source), self._entities(summary)
        novel = sm - es
        return min(1.0, len(novel) * 0.12 + (0.1 if len(sm) > len(es) + 5 else 0.0))

    def extractive_check(self, summary: str, source: str) -> Dict[str, Any]:
        sents = [s.strip() for s in re.split(r"(?<=[.!?])\s+", summary) if s.strip()]
        direct: List[str] = []
        src_l = source.lower()
        for s in sents:
            frag = s.lower()[:80]
            if frag and frag in src_l:
                direct.append(s)
        ratio = len(direct) / max(len(sents), 1)
        return {"extractive_sentences": direct, "extractive_ratio": round(float(ratio), 6)}

    def entity_check(self, summary: str, source: str) -> Dict[str, Any]:
        novel = sorted(self._entities(summary) - self._entities(source))
        return {"novel_entities": novel, "count": len(novel)}

    @staticmethod
    def length_ratio(summary: str, source: str) -> Dict[str, Any]:
        ls, lr = max(1, len(str(summary))), max(1, len(str(source)))
        r = ls / lr
        sane = 0.05 <= r <= 0.85
        sigma = max(0.0, min(1.0, abs(r - 0.25) * 1.2)) if sane else 0.55
        return {"chars_summary": ls, "chars_source": lr, "ratio": round(r, 6), "sigma_ratio": round(float(sigma), 6)}

    def multi_doc_sigma(
        self,
        summaries: Sequence[str],
        sources: Sequence[str],
        gate: Any,
    ) -> Dict[str, Any]:
        if len(summaries) != len(sources):
            return {"ok": False, "error": "length_mismatch"}
        sigs: List[float] = []
        for s, u in zip(summaries, sources):
            sigs.append(float(self.score_summary(u, s, gate)["sigma_combined"]))
        return {
            "ok": True,
            "per_doc_sigma": [round(x, 6) for x in sigs],
            "mean_sigma": round(sum(sigs) / len(sigs), 6),
        }
