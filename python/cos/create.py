# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Creative problem solving with σ: **creativity ≈ novelty × quality** (lab decomposition).

Boden-style labels (**combinatorial / exploratory / transformational**) classify novelty bands.
This does **not** replace human creative judgment or CreativeBench harness claims; see
``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import math
from typing import Any, Dict, List, Sequence

from cos.sigma_gate import SigmaGate

__all__ = ["SigmaCreate"]


def _verdict_str(verdict: Any) -> str:
    raw = str(getattr(verdict, "name", verdict))
    return raw.split(".")[-1] if "." in raw else raw


class SigmaCreate:
    """σ-validated creativity: gate scores **quality**; graph overlap proxies **novelty**."""

    def __init__(self, gate: Any = None, graph: Any = None) -> None:
        self.gate = gate or SigmaGate()
        self.graph = graph

    def generate_novel(
        self,
        seed: str,
        candidates: Sequence[str],
        context: str = "",
    ) -> List[Dict[str, Any]]:
        """Rank candidates by **creativity = novelty × quality** with **quality = 1 − σ**."""
        scored: List[Dict[str, Any]] = []
        seed_s = str(seed).strip()
        ctx = str(context).strip()
        prompt = f"{ctx}\n{seed_s}" if ctx else seed_s
        for candidate in candidates:
            cand = str(candidate)
            σ, verdict = self.gate.score(prompt, cand)
            quality = max(0.0, 1.0 - float(σ))
            novelty = float(self._novelty(cand))
            creativity = novelty * quality
            boden = self._classify_boden(cand, novelty)
            scored.append(
                {
                    "candidate": cand,
                    "σ": round(float(σ), 4),
                    "quality": round(quality, 4),
                    "novelty": round(novelty, 4),
                    "creativity": round(creativity, 4),
                    "boden_type": boden,
                    "verdict": _verdict_str(verdict),
                },
            )
        scored.sort(key=lambda x: x["creativity"], reverse=True)
        return scored

    def elegance_score(self, solution: str) -> float:
        """Elegance ≈ **quality / log2(complexity + 1)** (short, coherent solutions score higher)."""
        sol = str(solution).strip()
        σ, _ = self.gate.score("Evaluate this solution for coherence.", sol)
        quality = max(0.0, 1.0 - float(σ))
        complexity = len(sol.split()) if sol else 0
        if complexity == 0:
            return 0.0
        elegance = quality / math.log2(complexity + 1)
        return round(float(elegance), 4)

    def divergent_think(self, problem: str, n_directions: int = 5) -> List[Dict[str, Any]]:
        """Produce reframed *directions*, each σ-scored against the original problem."""
        prefixes = [
            "What if the opposite were true: ",
            "Combine this with something unrelated: ",
            "What would a child say about: ",
            "Remove the biggest constraint from: ",
            "What if time worked backwards in: ",
        ]
        prob = str(problem).strip()
        n = max(1, min(int(n_directions), len(prefixes)))
        directions: List[Dict[str, Any]] = []
        for prefix in prefixes[:n]:
            reframed = prefix + prob
            σ, verdict = self.gate.score(prob, reframed)
            directions.append(
                {
                    "direction": reframed,
                    "σ": round(float(σ), 4),
                    "verdict": _verdict_str(verdict),
                },
            )
        return directions

    def _novelty(self, candidate: str) -> float:
        """1 − (token overlap with graph entity labels); no graph ⇒ 0.5 (unknown)."""
        if not self.graph:
            return 0.5
        ent_fn = getattr(self.graph, "entities", None)
        if not callable(ent_fn):
            return 0.5
        entities: List[str] = list(ent_fn())
        words = str(candidate).lower().split()
        if not words:
            return 0.5
        if not entities:
            return 1.0
        elower = [e.lower() for e in entities]
        overlap = sum(1 for w in words if w in elower)
        ratio = overlap / float(len(words))
        return round(1.0 - ratio, 4)

    @staticmethod
    def _classify_boden(candidate: str, novelty: float) -> str:
        del candidate
        n = float(novelty)
        if n < 0.3:
            return "combinatorial"
        if n < 0.7:
            return "exploratory"
        return "transformational"
