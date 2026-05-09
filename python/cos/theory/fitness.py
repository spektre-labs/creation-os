# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Darwinian **metaphor** on σ: **fitness ≈ 1 − σ** in the gate’s coherence readout.

**Natural selection** is framed as **preferring lower-σ** variants under a fixed scoring
channel — a *substrate-neutral* story (Dennett’s “universal acid” **pedagogy** only). This is
**not** population genetics, **not** Fisher’s theorem proved for LLM weights, and **not**
AGI achieved. Directed σ-gated search in ``cos.evolve`` is the closest operational sibling;
this module is a **minimal, testable** scaffold. See ``docs/CLAIM_DISCIPLINE.md``.
"""
from __future__ import annotations

from typing import Any, Callable, Dict, List, Optional, Sequence, Union

from cos.sigma_gate import SigmaGate

__all__ = ["SigmaFitness"]


def _verdict_str(verdict: Any) -> str:
    return str(getattr(verdict, "name", verdict))


Genotype = Union[str, Any]
MutationFn = Callable[[Genotype], Genotype]


class SigmaFitness:
    """Population-level **ranking** and toy **select → vary** loop on gate σ."""

    def __init__(self, gate: Any = None) -> None:
        self.gate = gate or SigmaGate()
        self.population: List[Dict[str, Any]] = []
        self.generations = 0

    def fitness(self, genotype: Genotype, environment: str = "") -> Dict[str, Any]:
        """``fitness = max(0, 1 − σ)`` from :meth:`~cos.sigma_gate.SigmaGate.score`."""
        env = (environment or "").strip() or "fitness"
        sigma, verdict = self.gate.score(env, str(genotype))
        s = float(sigma)
        f = max(0.0, 1.0 - s)
        return {
            "genotype": genotype,
            "σ": round(s, 4),
            "sigma": round(s, 4),
            "fitness": round(f, 4),
            "verdict": _verdict_str(verdict),
        }

    def populate(self, genotypes: Sequence[Genotype], environment: str = "") -> List[Dict[str, Any]]:
        """Score every genotype and sort **descending** by fitness."""
        self.population = [self.fitness(g, environment) for g in genotypes]
        self.population.sort(key=lambda x: x["fitness"], reverse=True)
        return self.population

    def select(self, top_fraction: float = 0.5) -> List[Dict[str, Any]]:
        """Truncation selection: keep top ``top_fraction`` of the sorted population."""
        if not self.population:
            return []
        n = max(1, int(len(self.population) * float(top_fraction)))
        return self.population[:n]

    def vary(self, genotype: Genotype, mutation_fn: Optional[MutationFn] = None) -> Genotype:
        """Deterministic default mutation (append ``#``) — no RNG for CI stability."""
        if mutation_fn is not None:
            return mutation_fn(genotype)
        s = str(genotype)
        return s + "#" if len(s) < 4096 else s[:-1] + "#"

    def evolve(
        self,
        genotypes: Sequence[Genotype],
        environment: str = "",
        generations: int = 10,
        top_fraction: float = 0.5,
        mutation_fn: Optional[MutationFn] = None,
    ) -> Dict[str, Any]:
        """Populate → select → copy + mutate; σ **may** fall if the gate rewards mutants."""
        hist: List[Dict[str, Any]] = []
        current: List[Genotype] = list(genotypes)
        ngen = max(0, int(generations))

        for gen in range(ngen):
            if not current:
                break
            self.populate(current, environment)
            avg_s = sum(float(p["σ"]) for p in self.population) / len(self.population)
            best = self.population[0]
            hist.append(
                {
                    "generation": gen,
                    "avg_σ": round(avg_s, 4),
                    "best_fitness": best["fitness"],
                    "best_genotype": str(best["genotype"])[:50],
                    "pop_size": len(self.population),
                }
            )
            survivors = self.select(top_fraction)
            offspring: List[Genotype] = []
            for srow in survivors:
                g = srow["genotype"]
                offspring.append(g)
                offspring.append(self.vary(g, mutation_fn))
            current = offspring
            self.generations += 1

        improved = (
            len(hist) > 1 and hist[-1]["avg_σ"] < hist[0]["avg_σ"]
        )
        return {
            "generations": len(hist),
            "history": hist,
            "σ_start": hist[0]["avg_σ"] if hist else 0.0,
            "σ_end": hist[-1]["avg_σ"] if hist else 0.0,
            "improved": improved,
            "fundamental_theorem": (
                "avg fitness increases if variance > 0 (Fisher sketch; not proved for this gate)"
            ),
        }
