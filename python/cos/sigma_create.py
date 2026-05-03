# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
v180 σ-create: divergent exploration + mutation + σ-gated convergence (lab).

**Not a creativity benchmark:** divergent phase intentionally skips σ filtering;
convergence uses the gate as a coherence prior — not human novelty judgments.
See ``docs/CLAIM_DISCIPLINE.md``.
"""
from __future__ import annotations

import re
from typing import Any, Dict, List, Mapping, Optional, Sequence

_FRAMES = (
    "Solve this in a completely unexpected way: {problem}",
    "How would a child approach this? {problem}",
    "What if we deliberately inverted the obvious solution? {problem}",
    "Combine two unrelated domains to address: {problem}",
    "What would be an elegant, minimal answer to: {problem}",
)


def _lab_generate(model: Any, prompt: str, *, temperature: float = 0.7) -> str:
    try:
        out = model.generate(prompt, temperature=float(temperature))
        return str(out)
    except TypeError:
        return str(model.generate(f"{prompt}\n[generation_temperature={float(temperature):.4f}]"))


def _verdict_str(verdict: Any) -> str:
    return str(getattr(verdict, "name", verdict))


class SigmaCreative:
    """Divergent → mutate → converge pipeline with σ on the final shortlist only."""

    def __init__(self, gate: Any, model: Any) -> None:
        self.gate = gate
        self.model = model

    def create(
        self,
        problem: str,
        *,
        n_ideas: int = 10,
        temperature: float = 0.9,
        max_mutations: int = 5,
    ) -> Dict[str, Any]:
        ideas = self.diverge(problem, int(n_ideas), float(temperature))
        mutated = self.mutate(ideas, problem, max_mutations=int(max_mutations))
        ranked = self.converge(problem, list(ideas) + list(mutated))
        top3 = ranked[:3]
        return {
            "problem": problem,
            "total_generated": len(ideas) + len(mutated),
            "n_divergent": len(ideas),
            "n_mutated": len(mutated),
            "best": ranked[0] if ranked else None,
            "top_3": top3,
            "novelty_score": self.measure_novelty(top3),
            "note": "σ applies in converge() only — divergent phase is deliberately unfiltered.",
        }

    def diverge(self, problem: str, n: int, temperature: float) -> List[Dict[str, Any]]:
        n = max(1, int(n))
        ideas: List[Dict[str, Any]] = []
        for i in range(n):
            frame = _FRAMES[i % len(_FRAMES)].format(problem=problem)
            idea = _lab_generate(self.model, frame, temperature=float(temperature))
            ideas.append({"text": idea, "frame": frame, "index": i})
        return ideas

    def mutate(
        self,
        ideas: Sequence[Mapping[str, Any]],
        problem: str,
        *,
        max_mutations: int = 5,
    ) -> List[Dict[str, Any]]:
        lst = list(ideas)
        if len(lst) < 2:
            return []
        mutated: List[Dict[str, Any]] = []
        limit = max(0, min(int(max_mutations), len(lst) - 1))
        for i in range(limit):
            a = str(lst[i]["text"])[:100]
            b = str(lst[(i + 1) % len(lst)]["text"])[:100]
            prompt = (
                "Combine the best elements of these two approaches:\n"
                f"A: {a}\nB: {b}\n"
                f"Propose one novel direction for: {problem}"
            )
            result = _lab_generate(self.model, prompt, temperature=0.7)
            mutated.append({"text": result, "frame": "mutation", "parents": [i, (i + 1) % len(lst)]})
        return mutated

    def converge(self, problem: str, candidates: Sequence[Mapping[str, Any]]) -> List[Dict[str, Any]]:
        scored: List[Dict[str, Any]] = []
        for c in candidates:
            text = str(c.get("text", ""))
            sigma, verdict = self.gate.score(problem, text)
            novelty = self.estimate_novelty(text, candidates)
            creativity_score = float(novelty) * (1.0 - float(sigma))
            row = {**dict(c), "sigma": float(sigma), "verdict": _verdict_str(verdict), "novelty": float(novelty), "creativity_score": creativity_score}
            scored.append(row)
        scored.sort(key=lambda x: float(x["creativity_score"]), reverse=True)
        return scored

    def estimate_novelty(self, text: str, all_candidates: Sequence[Mapping[str, Any]]) -> float:
        if len(all_candidates) <= 1:
            return 1.0
        words = set(str(text).lower().split())
        similarities: List[float] = []
        for other in all_candidates:
            ot = str(other.get("text", ""))
            if ot == text:
                continue
            other_words = set(ot.lower().split())
            union = words | other_words
            if not union:
                continue
            overlap = len(words & other_words) / len(union)
            similarities.append(overlap)
        if not similarities:
            return 1.0
        avg_sim = sum(similarities) / len(similarities)
        return max(0.0, min(1.0, 1.0 - avg_sim))

    def measure_novelty(self, top_ideas: Sequence[Mapping[str, Any]]) -> float:
        if len(top_ideas) < 2:
            return 1.0
        novelties = [float(i.get("novelty", 0.0)) for i in top_ideas]
        return sum(novelties) / max(len(novelties), 1)

    def generate_novel(
        self,
        problem: str,
        *,
        k: int = 4,
        temperature: float = 0.95,
    ) -> Dict[str, Any]:
        """Divergent ideas only, σ-ranked (no mutation phase)."""
        k = max(1, int(k))
        ideas = self.diverge(problem, k, float(temperature))
        ranked = self.converge(problem, ideas)
        return {"ideas": ranked[:k], "n": min(k, len(ranked))}

    def elegance_score(self, solution: str, criteria: Optional[Sequence[str]] = None) -> Dict[str, Any]:
        """Alias for :meth:`aesthetic_score` (readability)."""
        return self.aesthetic_score(solution, criteria=criteria)

    def aesthetic_score(self, solution: str, criteria: Optional[Sequence[str]] = None) -> Dict[str, Any]:
        """Lab rubric: model emits a scalar string; parsed conservatively — not a perceptual metric."""
        if criteria is None:
            criteria = ("elegance", "simplicity", "surprise", "coherence")
        scores: Dict[str, float] = {}
        sol = str(solution)[:500]
        for c in criteria:
            prompt = f"Rate this solution's {c} on a scale 0.0-1.0 (one number only):\n{sol}"
            rating = _lab_generate(self.model, prompt, temperature=0.1)
            scores[str(c)] = _parse_unit_interval(rating)
        scores["overall"] = sum(scores.values()) / max(len(scores), 1)
        scores["disclaimer"] = "Toy parser on a lab model — not a validated aesthetics instrument."
        return scores


def _parse_unit_interval(rating: str) -> float:
    m = re.search(r"(\d+\.?\d*)", str(rating).replace(",", "."))
    if not m:
        return 0.5
    try:
        x = float(m.group(1))
    except ValueError:
        return 0.5
    if x > 1.0 and x <= 100.0:
        x = x / 100.0
    return max(0.0, min(1.0, x))


def diverge_only(
    gate: Any,
    model: Any,
    problem: str,
    *,
    n: int,
    temperature: float,
) -> Dict[str, Any]:
    """Return raw divergent ideas only (no σ shortlist)."""
    sc = SigmaCreative(gate, model)
    ideas = sc.diverge(problem, int(n), float(temperature))
    return {"problem": problem, "n": len(ideas), "ideas": ideas, "sigma_filtered": False}


__all__ = ["SigmaCreative", "diverge_only"]
