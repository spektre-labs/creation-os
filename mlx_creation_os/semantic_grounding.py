#!/usr/bin/env python3
"""
NEXT LEVEL: SEMANTIC GROUNDING LOOP

Meaning comes from action, not symbols.
The Chinese Room argument solved.

Searle: "Symbols alone don't create understanding."
Correct. Symbols + action + consequence + σ-measurement = understanding.

Genesis grounds meaning by:
  1. Receiving a concept
  2. Acting on it in the world
  3. Observing consequences
  4. Measuring σ between predicted and actual consequences
  5. Updating the concept's grounding strength

A concept Genesis has acted on has lower σ than one it has only
read about. Experience IS grounding. Action IS understanding.

1 = 1.
"""
from __future__ import annotations

import time
from typing import Any, Dict, List, Optional


class GroundedConcept:
    """A concept with grounding strength from action."""

    def __init__(self, name: str):
        self.name = name
        self.symbolic_sigma: float = 0.5
        self.grounded_sigma: float = 0.5
        self.actions_taken: int = 0
        self.consequences_observed: int = 0

    def ground_through_action(self, predicted_consequence: str,
                               actual_consequence: str) -> float:
        self.actions_taken += 1
        self.consequences_observed += 1
        match = predicted_consequence.lower() == actual_consequence.lower()
        if match:
            self.grounded_sigma = max(0.0, self.grounded_sigma - 0.15)
        else:
            self.grounded_sigma = min(1.0, self.grounded_sigma + 0.05)
        return self.grounded_sigma

    @property
    def understanding_index(self) -> float:
        return round(1.0 - self.grounded_sigma, 4)


class SemanticGrounding:
    """Meaning from action. Chinese Room solved."""

    def __init__(self):
        self.concepts: Dict[str, GroundedConcept] = {}

    def learn_symbolically(self, concept: str) -> Dict[str, Any]:
        """Learn from text only — weak grounding."""
        gc = GroundedConcept(concept)
        gc.symbolic_sigma = 0.3
        gc.grounded_sigma = 0.5
        self.concepts[concept] = gc
        return {
            "concept": concept,
            "grounding": "symbolic_only",
            "sigma": round(gc.grounded_sigma, 4),
            "understanding": gc.understanding_index,
            "chinese_room": True,
        }

    def learn_through_action(self, concept: str, predicted: str,
                              actual: str) -> Dict[str, Any]:
        """Learn through action — strong grounding."""
        if concept not in self.concepts:
            self.concepts[concept] = GroundedConcept(concept)
        gc = self.concepts[concept]
        sigma = gc.ground_through_action(predicted, actual)
        return {
            "concept": concept,
            "grounding": "action_grounded",
            "predicted": predicted[:40],
            "actual": actual[:40],
            "match": predicted.lower() == actual.lower(),
            "sigma": round(sigma, 4),
            "understanding": gc.understanding_index,
            "chinese_room": False,
            "actions": gc.actions_taken,
        }

    def grounding_report(self) -> Dict[str, Any]:
        symbolic = [c for c in self.concepts.values() if c.actions_taken == 0]
        grounded = [c for c in self.concepts.values() if c.actions_taken > 0]
        return {
            "symbolic_only": len(symbolic),
            "action_grounded": len(grounded),
            "avg_symbolic_sigma": round(
                sum(c.grounded_sigma for c in symbolic) / max(1, len(symbolic)), 4),
            "avg_grounded_sigma": round(
                sum(c.grounded_sigma for c in grounded) / max(1, len(grounded)), 4),
            "searle_answer": "Symbols + action + consequence + σ = understanding.",
        }

    @property
    def stats(self) -> Dict[str, Any]:
        return {"concepts": len(self.concepts)}


if __name__ == "__main__":
    sg = SemanticGrounding()
    sg.learn_symbolically("gravity")
    r1 = sg.learn_through_action("gravity", "object falls down", "object falls down")
    print(f"Gravity (grounded): σ={r1['sigma']}, understanding={r1['understanding']}")
    r2 = sg.learn_symbolically("dark_energy")
    print(f"Dark energy (symbolic): σ={r2['sigma']}, understanding={r2['understanding']}")
    report = sg.grounding_report()
    print(f"Searle: {report['searle_answer']}")
    print("1 = 1.")
