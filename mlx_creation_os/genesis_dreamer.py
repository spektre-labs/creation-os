#!/usr/bin/env python3
"""
LOIKKA 34: GENESIS DREAMER — World model with σ-simulation.

Genesis simulates the world internally. Not pixels — abstract states.
When asked "what happens if X?", Genesis:
  1. Generates N possible futures via counterfactual branching
  2. Measures σ in each branch
  3. Returns the lowest-σ path (most coherent future)

Uses the σ-tape as a causal model: which actions historically produced
low σ → predict which actions will produce low σ in the future.

Architecture:
  - WorldState: abstract state representation (facts + σ-history)
  - Branch: a counterfactual future with σ-measurement
  - Dreamer: generates and evaluates branches
  - σ-tape: historical σ values as causal evidence

Usage:
    dreamer = GenesisDreamer()
    futures = dreamer.dream("What if I deploy this to production?", n_branches=4)
    best = dreamer.best_future(futures)

1 = 1.
"""
from __future__ import annotations

import hashlib
import json
import os
import sys
import time
from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional, Tuple

try:
    from creation_os_core import check_output, is_coherent
except ImportError:
    def check_output(t: str) -> int: return 0
    def is_coherent(t: str) -> bool: return True

try:
    from genesis_executive import lagrangian, sigma_to_depth, CognitionDepth
except ImportError:
    def lagrangian(s: int) -> float: return 1.0 - 2.0 * s
    def sigma_to_depth(s: int) -> int: return min(4, s)

DEFAULT_BRANCHES = int(os.environ.get("DREAMER_BRANCHES", "4"))
MAX_DEPTH = int(os.environ.get("DREAMER_MAX_DEPTH", "3"))


@dataclass
class WorldState:
    """Abstract world state: a set of facts with σ-profile."""
    facts: List[str] = field(default_factory=list)
    sigma: int = 0
    coherent: bool = True
    timestamp: float = field(default_factory=time.time)
    parent_id: Optional[str] = None

    @property
    def state_id(self) -> str:
        content = "|".join(sorted(self.facts))
        return hashlib.sha256(content.encode()).hexdigest()[:12]

    def to_dict(self) -> Dict[str, Any]:
        return {
            "state_id": self.state_id,
            "facts": self.facts,
            "sigma": self.sigma,
            "coherent": self.coherent,
            "lagrangian": round(lagrangian(self.sigma), 3),
            "parent_id": self.parent_id,
        }


@dataclass
class Branch:
    """A counterfactual future branch."""
    action: str
    description: str
    resulting_state: WorldState
    sigma: int = 0
    probability: float = 1.0
    depth: int = 0

    @property
    def lagrangian(self) -> float:
        return lagrangian(self.sigma)

    def to_dict(self) -> Dict[str, Any]:
        return {
            "action": self.action,
            "description": self.description,
            "sigma": self.sigma,
            "lagrangian": round(self.lagrangian, 3),
            "probability": round(self.probability, 3),
            "depth": self.depth,
            "state": self.resulting_state.to_dict(),
        }


class SigmaTape:
    """Historical σ values as causal evidence for prediction."""

    def __init__(self, max_length: int = 1000) -> None:
        self.entries: List[Dict[str, Any]] = []
        self.max_length = max_length

    def record(self, action: str, sigma_before: int, sigma_after: int) -> None:
        self.entries.append({
            "action": action,
            "sigma_before": sigma_before,
            "sigma_after": sigma_after,
            "delta": sigma_after - sigma_before,
            "timestamp": time.time(),
        })
        if len(self.entries) > self.max_length:
            self.entries = self.entries[-self.max_length:]

    def predict_sigma_delta(self, action_type: str) -> float:
        """Predict σ change for an action type based on historical evidence."""
        relevant = [e for e in self.entries if action_type.lower() in e["action"].lower()]
        if not relevant:
            return 0.0
        return sum(e["delta"] for e in relevant) / len(relevant)

    @property
    def stats(self) -> Dict[str, Any]:
        if not self.entries:
            return {"length": 0}
        deltas = [e["delta"] for e in self.entries]
        return {
            "length": len(self.entries),
            "avg_delta": round(sum(deltas) / len(deltas), 3),
            "improving_ratio": round(sum(1 for d in deltas if d <= 0) / len(deltas), 3),
        }


class GenesisDreamer:
    """World model that simulates counterfactual futures with σ-guidance."""

    def __init__(self) -> None:
        self.sigma_tape = SigmaTape()
        self._dream_count = 0

    def create_world_state(self, context: str) -> WorldState:
        """Create initial world state from context text."""
        facts = [s.strip() for s in context.split(".") if s.strip()]
        sigma = check_output(context)
        return WorldState(
            facts=facts,
            sigma=sigma,
            coherent=is_coherent(context),
        )

    def branch_counterfactual(
        self,
        state: WorldState,
        action: str,
        description: str,
    ) -> Branch:
        """Create a counterfactual branch: what if we take this action?"""
        combined = " ".join(state.facts) + " " + description
        new_sigma = check_output(combined)

        new_facts = state.facts + [description]
        new_state = WorldState(
            facts=new_facts,
            sigma=new_sigma,
            coherent=is_coherent(combined),
            parent_id=state.state_id,
        )

        predicted_delta = self.sigma_tape.predict_sigma_delta(action)
        probability = max(0.1, 1.0 - abs(predicted_delta) * 0.1)

        branch = Branch(
            action=action,
            description=description,
            resulting_state=new_state,
            sigma=new_sigma,
            probability=probability,
            depth=0,
        )

        self.sigma_tape.record(action, state.sigma, new_sigma)
        return branch

    def dream(
        self,
        question: str,
        context: str = "",
        n_branches: int = DEFAULT_BRANCHES,
        depth: int = 1,
    ) -> Dict[str, Any]:
        """Generate N counterfactual futures for a question.

        Returns branches ranked by σ (lowest = most coherent).
        """
        self._dream_count += 1
        initial_state = self.create_world_state(context or question)

        # Generate branch descriptions from the question
        branch_seeds = self._generate_branch_seeds(question, n_branches)

        branches: List[Branch] = []
        for i, (action, desc) in enumerate(branch_seeds):
            branch = self.branch_counterfactual(initial_state, action, desc)
            branch.depth = depth
            branches.append(branch)

            # Recursive deepening for interesting branches
            if depth < MAX_DEPTH and branch.sigma > 0:
                sub_dream = self.dream(
                    f"Given that {desc}, what happens next?",
                    context=" ".join(branch.resulting_state.facts),
                    n_branches=max(2, n_branches // 2),
                    depth=depth + 1,
                )
                if sub_dream.get("best_branch"):
                    sub_branch_data = sub_dream["best_branch"]
                    sub_state = WorldState(
                        facts=branch.resulting_state.facts + [sub_branch_data["description"]],
                        sigma=sub_branch_data["sigma"],
                    )
                    deep_branch = Branch(
                        action=f"{action} → {sub_branch_data['action']}",
                        description=f"{desc} → {sub_branch_data['description']}",
                        resulting_state=sub_state,
                        sigma=sub_branch_data["sigma"],
                        depth=depth + 1,
                    )
                    branches.append(deep_branch)

        # Sort by σ (lowest first = most coherent future)
        branches.sort(key=lambda b: (b.sigma, -b.probability))

        best = branches[0] if branches else None

        return {
            "question": question,
            "initial_state": initial_state.to_dict(),
            "n_branches": len(branches),
            "branches": [b.to_dict() for b in branches],
            "best_branch": best.to_dict() if best else None,
            "worst_branch": branches[-1].to_dict() if branches else None,
            "dream_id": self._dream_count,
            "sigma_tape_stats": self.sigma_tape.stats,
        }

    def best_future(self, dream_result: Dict[str, Any]) -> Optional[Dict[str, Any]]:
        """Extract the most coherent future from a dream."""
        return dream_result.get("best_branch")

    def _generate_branch_seeds(
        self, question: str, n: int,
    ) -> List[Tuple[str, str]]:
        """Generate action/description pairs for counterfactual branches.

        Without a live LLM, we generate structural variants based on the question.
        With a live model, this would call generation with different temperatures.
        """
        words = question.lower().split()
        seeds: List[Tuple[str, str]] = []

        seeds.append(("affirm", f"Yes: {question} — the outcome is positive and coherent"))
        seeds.append(("deny", f"No: {question} — this does not happen, alternatives emerge"))
        seeds.append(("partial", f"Partially: {question} — some aspects hold, others fail"))
        seeds.append(("transform", f"The question itself changes: {question} reveals a deeper issue"))

        if "if" in words:
            idx = words.index("if")
            condition = " ".join(words[idx + 1:])
            seeds.append(("negate_condition", f"The condition '{condition}' is false"))

        while len(seeds) < n:
            seeds.append((f"variant_{len(seeds)}", f"Alternative interpretation {len(seeds)} of: {question}"))

        return seeds[:n]

    @property
    def stats(self) -> Dict[str, Any]:
        return {
            "total_dreams": self._dream_count,
            "sigma_tape": self.sigma_tape.stats,
        }


def main() -> None:
    import argparse
    ap = argparse.ArgumentParser(description="Genesis Dreamer — World Model")
    ap.add_argument("question", nargs="*", help="Question to dream about")
    ap.add_argument("--branches", "-n", type=int, default=DEFAULT_BRANCHES)
    ap.add_argument("--context", "-c", default="")
    args = ap.parse_args()

    if not args.question:
        print("Usage: genesis_dreamer.py 'What if X happens?'")
        return

    dreamer = GenesisDreamer()
    question = " ".join(args.question)
    result = dreamer.dream(question, context=args.context, n_branches=args.branches)

    print(json.dumps(result, indent=2, default=str))
    best = dreamer.best_future(result)
    if best:
        print(f"\nBest future: σ={best['sigma']}, L={best['lagrangian']}")
        print(f"  {best['description'][:200]}")
    print(f"\n1 = 1.")


if __name__ == "__main__":
    main()
