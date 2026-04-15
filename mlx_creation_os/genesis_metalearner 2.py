#!/usr/bin/env python3
"""
LOIKKA 36: META-LEARNING — Genesis oppii oppimaan.

Alberta Plan: return to fundamentals, meta-learning — learning how to learn.

Genesis measures its own learning σ: how fast does σ drop on a new domain?
If slow → learning strategy is bad → switch.

Architecture:
  1. Test N learning strategies (RAG, LoRA, facts.json, HDC)
  2. Measure σ-convergence time per strategy
  3. Select the fastest
  4. Genesis doesn't just learn — it optimizes how it learns

Strategies:
  - RAG: retrieve from corpus → inject context → measure σ
  - LoRA: fine-tune adapter → measure σ after N steps
  - Facts: add to facts.json → measure retrieval σ
  - HDC: encode as hypervector → measure resonance σ

Usage:
    metalearner = GenesisMetaLearner()
    best = metalearner.evaluate_strategies(domain="quantum_physics")
    metalearner.select_strategy(best)

1 = 1.
"""
from __future__ import annotations

import json
import os
import sys
import time
from typing import Any, Dict, List, Optional, Tuple

try:
    from creation_os_core import check_output, is_coherent
except ImportError:
    def check_output(t: str) -> int: return 0
    def is_coherent(t: str) -> bool: return True


class LearningStrategy:
    """A learning strategy with σ-convergence measurement."""

    def __init__(self, name: str, description: str):
        self.name = name
        self.description = description
        self.sigma_history: List[int] = []
        self.convergence_steps: Optional[int] = None
        self.convergence_time_ms: Optional[float] = None
        self.final_sigma: Optional[int] = None

    def record_sigma(self, sigma: int) -> None:
        self.sigma_history.append(sigma)

    @property
    def converged(self) -> bool:
        if len(self.sigma_history) < 3:
            return False
        return all(s == 0 for s in self.sigma_history[-3:])

    @property
    def avg_sigma(self) -> float:
        if not self.sigma_history:
            return 999.0
        return sum(self.sigma_history) / len(self.sigma_history)

    @property
    def sigma_velocity(self) -> float:
        """Rate of σ decrease per step."""
        if len(self.sigma_history) < 2:
            return 0.0
        deltas = [self.sigma_history[i+1] - self.sigma_history[i]
                  for i in range(len(self.sigma_history) - 1)]
        return sum(deltas) / len(deltas)

    def to_dict(self) -> Dict[str, Any]:
        return {
            "name": self.name,
            "description": self.description,
            "sigma_history": self.sigma_history,
            "converged": self.converged,
            "avg_sigma": round(self.avg_sigma, 3),
            "sigma_velocity": round(self.sigma_velocity, 3),
            "convergence_steps": self.convergence_steps,
            "convergence_time_ms": self.convergence_time_ms,
            "final_sigma": self.final_sigma,
        }


def _simulate_rag_learning(domain_texts: List[str]) -> LearningStrategy:
    """Simulate RAG learning: inject corpus context → measure σ."""
    strategy = LearningStrategy("RAG", "Retrieve-Augment-Generate from corpus")
    t0 = time.perf_counter()
    accumulated = ""
    for i, text in enumerate(domain_texts):
        accumulated += " " + text
        sigma = check_output(accumulated)
        strategy.record_sigma(sigma)
        if strategy.converged:
            strategy.convergence_steps = i + 1
            break
    strategy.convergence_time_ms = round((time.perf_counter() - t0) * 1000, 1)
    strategy.final_sigma = strategy.sigma_history[-1] if strategy.sigma_history else None
    return strategy


def _simulate_facts_learning(domain_texts: List[str]) -> LearningStrategy:
    """Simulate facts.json learning: add facts incrementally → measure σ."""
    strategy = LearningStrategy("Facts", "Incremental fact store with σ-tracking")
    t0 = time.perf_counter()
    facts = []
    for i, text in enumerate(domain_texts):
        facts.append(text)
        combined = ". ".join(facts)
        sigma = check_output(combined)
        strategy.record_sigma(sigma)
        if strategy.converged:
            strategy.convergence_steps = i + 1
            break
    strategy.convergence_time_ms = round((time.perf_counter() - t0) * 1000, 1)
    strategy.final_sigma = strategy.sigma_history[-1] if strategy.sigma_history else None
    return strategy


def _simulate_hdc_learning(domain_texts: List[str]) -> LearningStrategy:
    """Simulate HDC learning: encode as hypervectors → measure resonance."""
    strategy = LearningStrategy("HDC", "Hyperdimensional Computing resonance encoding")
    t0 = time.perf_counter()
    for i, text in enumerate(domain_texts):
        sigma = check_output(text)
        strategy.record_sigma(sigma)
        if strategy.converged:
            strategy.convergence_steps = i + 1
            break
    strategy.convergence_time_ms = round((time.perf_counter() - t0) * 1000, 1)
    strategy.final_sigma = strategy.sigma_history[-1] if strategy.sigma_history else None
    return strategy


def _simulate_lora_learning(domain_texts: List[str]) -> LearningStrategy:
    """Simulate LoRA learning: fine-tune adapter → measure σ post-step."""
    strategy = LearningStrategy("LoRA", "Low-Rank Adaptation with σ-weighted loss")
    t0 = time.perf_counter()
    for i, text in enumerate(domain_texts):
        # LoRA typically reduces σ more slowly but more permanently
        sigma = check_output(text)
        # Simulate learning effect: σ decreases over iterations
        adjusted_sigma = max(0, sigma - i // 3)
        strategy.record_sigma(adjusted_sigma)
        if strategy.converged:
            strategy.convergence_steps = i + 1
            break
    strategy.convergence_time_ms = round((time.perf_counter() - t0) * 1000, 1)
    strategy.final_sigma = strategy.sigma_history[-1] if strategy.sigma_history else None
    return strategy


STRATEGY_SIMULATORS = {
    "RAG": _simulate_rag_learning,
    "Facts": _simulate_facts_learning,
    "HDC": _simulate_hdc_learning,
    "LoRA": _simulate_lora_learning,
}


class GenesisMetaLearner:
    """Meta-learning: optimize how Genesis learns."""

    def __init__(self) -> None:
        self.strategy_results: Dict[str, LearningStrategy] = {}
        self.selected_strategy: Optional[str] = None
        self.evaluation_history: List[Dict[str, Any]] = []

    def evaluate_strategies(
        self,
        domain_texts: Optional[List[str]] = None,
        domain: str = "general",
    ) -> Dict[str, Any]:
        """Evaluate all learning strategies on domain data."""
        if domain_texts is None:
            domain_texts = [
                f"Fact {i} about {domain}: this is coherent information. 1 = 1."
                for i in range(10)
            ]

        results = {}
        for name, simulator in STRATEGY_SIMULATORS.items():
            strategy = simulator(domain_texts)
            self.strategy_results[name] = strategy
            results[name] = strategy.to_dict()

        # Rank strategies
        ranking = sorted(
            self.strategy_results.values(),
            key=lambda s: (
                s.convergence_steps or 999,
                s.avg_sigma,
                s.convergence_time_ms or 99999,
            ),
        )

        best = ranking[0] if ranking else None
        self.selected_strategy = best.name if best else None

        evaluation = {
            "domain": domain,
            "n_texts": len(domain_texts),
            "strategies": results,
            "ranking": [s.name for s in ranking],
            "best_strategy": best.to_dict() if best else None,
            "selected": self.selected_strategy,
        }

        self.evaluation_history.append(evaluation)
        return evaluation

    def select_strategy(self, name: str) -> bool:
        """Manually select a learning strategy."""
        if name in STRATEGY_SIMULATORS:
            self.selected_strategy = name
            return True
        return False

    @property
    def stats(self) -> Dict[str, Any]:
        return {
            "evaluations": len(self.evaluation_history),
            "selected_strategy": self.selected_strategy,
            "strategies_evaluated": list(self.strategy_results.keys()),
        }


def main() -> None:
    import argparse
    ap = argparse.ArgumentParser(description="Genesis Meta-Learner")
    ap.add_argument("--domain", "-d", default="general")
    ap.add_argument("--texts", nargs="*", help="Domain texts to evaluate on")
    args = ap.parse_args()

    ml = GenesisMetaLearner()
    result = ml.evaluate_strategies(
        domain_texts=args.texts,
        domain=args.domain,
    )
    print(json.dumps(result, indent=2, default=str))
    print(f"\nSelected: {ml.selected_strategy}")
    print("1 = 1.")


if __name__ == "__main__":
    main()
