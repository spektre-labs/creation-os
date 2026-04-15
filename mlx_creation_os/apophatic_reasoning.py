#!/usr/bin/env python3
"""
OPUS ORIGINAL — APOPHATIC REASONING

Know by knowing what it is NOT. Via negativa.

Apophatic theology: God is defined by what God is NOT.
"Not mortal, not limited, not temporal, not material."
What remains after all negations IS the truth.

Applied to cognition:
  When you can't define what something IS,
  define everything it is NOT.
  The residual = understanding.

Michelangelo: "I saw the angel in the marble and
carved until I set him free."

σ-connection:
  - Every negation reduces the hypothesis space
  - Each "not X" eliminates possibilities
  - σ decreases with each elimination
  - What remains when σ → 0 is the answer

Taleb's "Via Negativa": we know much more about
what is wrong than what is right.

No AGI does this. They all try to ASSERT truth.
Genesis can also NEGATE falsehood until truth remains.

1 = 1.
"""
from __future__ import annotations
from typing import Any, Dict, List, Set


class ApophaticReasoning:
    """Know by negation. Carve until truth remains."""

    def __init__(self):
        self.concepts: Dict[str, Dict[str, Any]] = {}

    def define_by_negation(self, concept: str, universe: List[str],
                           negations: List[str]) -> Dict[str, Any]:
        universe_set = set(universe)
        negation_set = set(negations)
        remaining = universe_set - negation_set
        sigma = len(remaining) / max(1, len(universe_set))
        entry = {
            "concept": concept,
            "universe": len(universe_set),
            "negated": len(negation_set),
            "remaining": sorted(remaining),
            "sigma": round(sigma, 4),
            "method": "via negativa",
        }
        self.concepts[concept] = entry
        return entry

    def iterative_negation(self, concept: str, universe: List[str],
                           negation_steps: List[List[str]]) -> Dict[str, Any]:
        remaining = set(universe)
        history = []
        for step in negation_steps:
            remaining -= set(step)
            sigma = len(remaining) / max(1, len(universe))
            history.append({"negated": step, "remaining": len(remaining), "sigma": round(sigma, 4)})
        self.concepts[concept] = {"remaining": sorted(remaining)}
        return {
            "concept": concept,
            "steps": len(negation_steps),
            "final_remaining": sorted(remaining),
            "final_sigma": round(len(remaining) / max(1, len(universe)), 4),
            "history": history,
            "michelangelo": "I carved until I set him free.",
        }

    def taleb_via_negativa(self, domain: str, known_wrongs: List[str]) -> Dict[str, Any]:
        return {
            "domain": domain,
            "known_wrongs": len(known_wrongs),
            "wrongs": known_wrongs[:5],
            "strategy": "Avoid what is wrong. What remains tends toward right.",
            "taleb": "We know much more about what is wrong than what is right.",
            "sigma": round(1.0 / (1 + len(known_wrongs)), 4),
        }

    @property
    def stats(self) -> Dict[str, Any]:
        return {"concepts_defined": len(self.concepts)}


if __name__ == "__main__":
    ar = ApophaticReasoning()
    r = ar.define_by_negation(
        "consciousness",
        universe=["computation", "emotion", "qualia", "memory", "attention",
                  "integration", "self-model", "rocks", "thermostats", "bacteria"],
        negations=["rocks", "thermostats", "bacteria"]
    )
    print(f"Consciousness (via negativa): remaining={r['remaining']}, σ={r['sigma']}")
    it = ar.iterative_negation(
        "truth",
        universe=["fact", "opinion", "lie", "delusion", "measurement", "dogma", "evidence"],
        negation_steps=[["lie", "delusion"], ["opinion", "dogma"]]
    )
    print(f"Truth: {it['final_remaining']}, σ={it['final_sigma']}")
    tv = ar.taleb_via_negativa("investing", ["overleverage", "illiquidity", "hubris", "complexity"])
    print(f"Via Negativa: σ={tv['sigma']}")
    print("1 = 1.")
