# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
**Proconductor** multi-deputy debate: each model answers once, σ is measured per answer,
then a cheap **exact-text** consensus cluster is chosen with the lowest mean σ.

**Design note (in-tree lore):** the consensus rule mirrors the Creation OS motif that
independent routes converging on the same *place* (here: normalized string) support
corroboration — encoded as ``>= 2`` models in the lowest-σ cluster. Swap
:meth:`cluster_answers` for embeddings in research harnesses.

See ``docs/CLAIM_DISCIPLINE.md``.
"""
from __future__ import annotations

from collections import defaultdict
from statistics import mean
from typing import Any, Dict, List, Optional, Tuple


def _model_name(model: Any) -> str:
    n = getattr(model, "name", None)
    if isinstance(n, str) and n.strip():
        return n.strip()
    return type(model).__name__


class ProconductorDebate:
    """N-way single-shot answers + σ; cluster by normalized text; lowest-mean-σ cluster wins."""

    def __init__(self, models: List[Any], gate: Any) -> None:
        self.models = list(models)
        self.gate = gate

    def cluster_answers(self, results: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
        """
        Default: partition by **normalized** answer text (strip + lower + collapse spaces).

        Each cluster: ``{"answer": str, "mean_sigma": float, "members": list[result_dict]}``.
        """
        buckets: Dict[str, List[Dict[str, Any]]] = defaultdict(list)
        for row in results:
            ans = str(row.get("answer", "")).strip()
            key = " ".join(ans.lower().split())
            buckets[key].append(row)

        clusters: List[Dict[str, Any]] = []
        for _k, members in buckets.items():
            if not members:
                continue
            sigs = [max(0.0, min(1.0, float(m.get("sigma", 0.5)))) for m in members]
            clusters.append(
                {
                    "answer": str(members[0].get("answer", "")),
                    "mean_sigma": float(mean(sigs)) if sigs else 0.5,
                    "members": members,
                }
            )
        return clusters

    def multi_debate(self, question: str) -> Tuple[Optional[str], Optional[float]]:
        q = (question or "").strip()
        if not q or not self.models:
            return None, None

        results: List[Dict[str, Any]] = []
        for model in self.models:
            gen = getattr(model, "generate", None)
            if not callable(gen):
                continue
            try:
                ans = str(gen(q)).strip()
            except TypeError:
                ans = str(gen(q, temperature=0.7)).strip()
            if not ans:
                continue
            sigma = float(self.gate.compute_sigma(model, q, ans))
            sigma = max(0.0, min(1.0, sigma))
            results.append({"model": _model_name(model), "answer": ans, "sigma": sigma})

        if not results:
            return None, None

        clusters = self.cluster_answers(results)
        if not clusters:
            return None, None

        best = min(clusters, key=lambda c: float(c["mean_sigma"]))
        if len(best["members"]) >= 2:
            return str(best["answer"]), float(best["mean_sigma"])
        return None, None


__all__ = ["ProconductorDebate"]
