# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# Source:        https://github.com/spektre-labs/creation-os-kernel
# Website:       https://spektrelabs.org
# Commercial:    spektre.labs@proton.me
# License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
"""Lab NAS-style **module configuration search** scored by σ (not a full neural architecture search).

Candidate **dictionaries** (layer → component name) are turned into a short natural-language
**prompt** for :class:`~cos.sigma_gate.SigmaGate`; average σ over probe strings is the fitness proxy.
The C kernel / ``sigma_gate.h`` policy floor is unchanged—only Python lab bookkeeping runs here.

**NOT AGI ACHIEVED** — toy search space + entropy gate only; no claim of runtime self-rewiring on silicon.
"""
from __future__ import annotations

import random
from typing import Any, Dict, List, Mapping, Optional, Sequence

from cos.sigma_gate import SigmaGate

__all__ = ["ModuleSpec", "SigmaArchitect"]


class ModuleSpec:
    """Lightweight record for a named bundle of component slots (optional use beside dict candidates)."""

    def __init__(
        self,
        name: str,
        components: Sequence[str],
        config: Optional[Mapping[str, Any]] = None,
    ) -> None:
        self.name = str(name)
        self.components = list(components)
        self.config: Dict[str, Any] = dict(config or {})
        self.sigma: float = 1.0
        self.tested: bool = False

    def __repr__(self) -> str:
        return f"ModuleSpec({self.name!r}, σ={self.sigma:.3f}, n={len(self.components)})"


class SigmaArchitect:
    """Enumerate / evolve discrete layer→component maps with σ as a scalar fitness proxy."""

    def __init__(
        self,
        gate: Any = None,
        rng: Optional[random.Random] = None,
    ) -> None:
        self.gate = gate if gate is not None else SigmaGate()
        self.rng = rng if rng is not None else random.Random()
        self.search_space: Dict[str, List[str]] = {}
        self.architectures_tested: List[Dict[str, Any]] = []
        self.best_architecture: Optional[Dict[str, str]] = None
        self.best_sigma: float = 1.0

    def define_search_space(self, layer: str, options: Sequence[str]) -> None:
        """Register allowable component ids for one logical layer."""
        self.search_space[str(layer)] = [str(o) for o in options]

    def generate_candidate(self) -> Dict[str, str]:
        """Uniform random one choice per layer (requires non-empty search space)."""
        candidate: Dict[str, str] = {}
        for layer, options in self.search_space.items():
            candidate[layer] = self.rng.choice(options)
        return candidate

    def _describe(self, architecture: Mapping[str, str]) -> str:
        parts = [f"{k}:{v}" for k, v in sorted(architecture.items())]
        return " + ".join(parts)

    def evaluate(
        self,
        architecture: Mapping[str, str],
        test_prompts: Sequence[str],
    ) -> Dict[str, Any]:
        """Average σ across probes; lower is treated as better (lab convention)."""
        arch = {str(k): str(v) for k, v in architecture.items()}
        comps = self._describe(arch)
        prompts = [str(p) for p in test_prompts]
        σ_total = 0.0
        for prompt in prompts:
            gate_prompt = f"architecture [{comps}] processes"
            σ, _ = self.gate.score(gate_prompt, prompt)
            σ_total += float(σ)

        denom = max(len(prompts), 1)
        avg_σ = σ_total / denom
        result = {
            "architecture": arch,
            "avg_σ": round(float(avg_σ), 4),
            "n_tests": len(prompts),
        }
        self.architectures_tested.append(result)

        if avg_σ < self.best_sigma:
            self.best_sigma = float(avg_σ)
            self.best_architecture = dict(arch)

        return result

    def search(
        self,
        test_prompts: Sequence[str],
        n_candidates: int = 20,
        n_generations: int = 5,
        converge_threshold: float = 0.3,
    ) -> Dict[str, Any]:
        """Simple generational loop: evaluate → rank → keep top half → mutate one layer per survivor."""
        if not self.search_space:
            return {
                "best_architecture": None,
                "best_σ": 1.0,
                "generations": 0,
                "total_evaluated": len(self.architectures_tested),
                "history": [],
                "converged": False,
                "error": "empty search_space",
            }

        n_candidates = max(2, int(n_candidates))
        n_generations = max(1, int(n_generations))
        population = [self.generate_candidate() for _ in range(n_candidates)]
        history: List[Dict[str, Any]] = []

        for gen in range(n_generations):
            scored: List[tuple[Dict[str, str], float]] = []
            for arch in population:
                result = self.evaluate(arch, test_prompts)
                scored.append((dict(result["architecture"]), float(result["avg_σ"])))

            scored.sort(key=lambda x: x[1])
            best_gen_σ = scored[0][1]
            history.append(
                {
                    "generation": gen,
                    "best_σ": round(float(best_gen_σ), 4),
                    "avg_σ": round(float(sum(s[1] for s in scored) / len(scored)), 4),
                }
            )

            half = max(1, n_candidates // 2)
            survivors = [dict(s[0]) for s in scored[:half]]

            offspring: List[Dict[str, str]] = []
            layers = list(self.search_space.keys())
            for parent in survivors:
                child = dict(parent)
                layer = self.rng.choice(layers)
                child[layer] = self.rng.choice(self.search_space[layer])
                offspring.append(child)

            population = survivors + offspring
            while len(population) < n_candidates:
                population.append(self.generate_candidate())
            population = population[:n_candidates]

        return {
            "best_architecture": self.best_architecture,
            "best_σ": round(float(self.best_sigma), 4),
            "generations": n_generations,
            "total_evaluated": len(self.architectures_tested),
            "history": history,
            "converged": float(self.best_sigma) < converge_threshold,
        }

    def hot_swap(
        self,
        current_arch: Mapping[str, str],
        layer: str,
        new_component: str,
        test_prompts: Sequence[str],
    ) -> Dict[str, Any]:
        """Score current vs one-layer swap; accept iff σ does not increase (non-strict improvement ok)."""
        r0 = self.evaluate(dict(current_arch), test_prompts)
        σ_before = float(r0["avg_σ"])

        new_arch = dict(current_arch)
        new_arch[str(layer)] = str(new_component)
        r1 = self.evaluate(new_arch, test_prompts)
        σ_after = float(r1["avg_σ"])

        accept = σ_after <= σ_before
        return {
            "layer": str(layer),
            "old": current_arch.get(layer),
            "new": str(new_component),
            "σ_before": round(σ_before, 4),
            "σ_after": round(σ_after, 4),
            "accepted": accept,
            "improvement": round(σ_before - σ_after, 4),
        }

    def auto_optimize(
        self,
        current_arch: Mapping[str, str],
        test_prompts: Sequence[str],
    ) -> Dict[str, Any]:
        """Greedy coordinate sweep: for each layer try every option; pick lowest mean σ."""
        optimized: Dict[str, str] = {str(k): str(v) for k, v in current_arch.items()}
        improvements: List[Dict[str, Any]] = []

        for layer, options in self.search_space.items():
            if layer not in optimized:
                optimized[layer] = self.rng.choice(options)
            best_option = optimized[layer]
            best_layer_σ = float("inf")

            for option in options:
                test_arch = dict(optimized)
                test_arch[layer] = option
                r = self.evaluate(test_arch, test_prompts)
                σ = float(r["avg_σ"])
                if σ < best_layer_σ:
                    best_layer_σ = σ
                    best_option = option

            if best_option != optimized[layer]:
                improvements.append(
                    {
                        "layer": layer,
                        "old": optimized[layer],
                        "new": best_option,
                        "σ_improvement": round(float(best_layer_σ), 4),
                    }
                )
                optimized[layer] = best_option

        final_r = self.evaluate(optimized, test_prompts)
        return {
            "optimized": optimized,
            "improvements": improvements,
            "n_improved": len(improvements),
            "final_σ": round(float(final_r["avg_σ"]), 4),
        }
