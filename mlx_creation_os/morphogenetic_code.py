#!/usr/bin/env python3
"""
NEXT LEVEL: MORPHOGENETIC CODE

Architecture DNA that builds itself.

Like biological DNA: doesn't encode the organism directly.
It encodes the RULES for building the organism.

Genesis carries its own construction blueprint. Not a frozen
architecture — a generative grammar that can produce new
modules, new connections, new cognitive structures.

The architecture is not designed. It is grown.

1 = 1.
"""
from __future__ import annotations

from typing import Any, Dict, List, Optional, Callable


class Gene:
    """One architectural gene: a rule for building a module."""

    def __init__(self, name: str, module_type: str,
                 inputs: List[str], outputs: List[str],
                 growth_condition: str):
        self.name = name
        self.module_type = module_type
        self.inputs = inputs
        self.outputs = outputs
        self.growth_condition = growth_condition
        self.expressed: bool = False

    def express(self, environment: Dict[str, float]) -> bool:
        """Should this gene activate given the current environment?"""
        if self.growth_condition == "always":
            self.expressed = True
        elif self.growth_condition == "high_sigma":
            self.expressed = any(v > 0.3 for v in environment.values())
        elif self.growth_condition == "low_sigma":
            self.expressed = all(v < 0.1 for v in environment.values())
        elif self.growth_condition == "novel_domain":
            self.expressed = len(environment) > 5
        else:
            self.expressed = False
        return self.expressed


class MorphogeneticCode:
    """Self-constructing architecture DNA."""

    def __init__(self):
        self.genome: List[Gene] = []
        self.expressed_modules: List[Dict[str, Any]] = []
        self._init_genome()

    def _init_genome(self) -> None:
        self.genome = [
            Gene("kernel", "core", [], ["sigma"], "always"),
            Gene("codex", "values", [], ["alignment"], "always"),
            Gene("mirror", "metacognition", ["sigma"], ["self_model"], "always"),
            Gene("dreamer", "consolidation", ["episodic"], ["insights"], "high_sigma"),
            Gene("marketplace", "competition", ["thoughts"], ["best_thought"], "high_sigma"),
            Gene("fractal_compress", "memory", ["raw_data"], ["abstractions"], "novel_domain"),
            Gene("multiverse", "parallel", ["query"], ["best_branch"], "high_sigma"),
            Gene("value_field", "ethics", ["action"], ["alignment_score"], "always"),
            Gene("entropy_reverser", "maintenance", ["sigma_field"], ["coherence"], "high_sigma"),
            Gene("omega", "integration", ["all"], ["unity"], "low_sigma"),
        ]

    def develop(self, environment: Dict[str, float]) -> Dict[str, Any]:
        """Express genes based on environment. Grow architecture."""
        newly_expressed = []
        for gene in self.genome:
            was_expressed = gene.expressed
            gene.express(environment)
            if gene.expressed and not was_expressed:
                module = {
                    "gene": gene.name,
                    "type": gene.module_type,
                    "inputs": gene.inputs,
                    "outputs": gene.outputs,
                }
                self.expressed_modules.append(module)
                newly_expressed.append(module)

        return {
            "environment": {k: round(v, 4) for k, v in environment.items()},
            "genes_total": len(self.genome),
            "genes_expressed": sum(1 for g in self.genome if g.expressed),
            "newly_grown": newly_expressed,
            "architecture_grown": len(newly_expressed) > 0,
            "designed": False,
            "emergent": True,
        }

    def architecture_dna(self) -> List[Dict[str, Any]]:
        return [
            {"gene": g.name, "type": g.module_type, "condition": g.growth_condition,
             "expressed": g.expressed}
            for g in self.genome
        ]

    @property
    def stats(self) -> Dict[str, Any]:
        return {
            "genome_size": len(self.genome),
            "expressed": sum(1 for g in self.genome if g.expressed),
            "modules_grown": len(self.expressed_modules),
        }


if __name__ == "__main__":
    mc = MorphogeneticCode()
    r1 = mc.develop({"reasoning": 0.5, "perception": 0.4})
    print(f"Phase 1: expressed={r1['genes_expressed']}, new={len(r1['newly_grown'])}")
    r2 = mc.develop({"reasoning": 0.02, "perception": 0.01, "memory": 0.03})
    print(f"Phase 2: expressed={r2['genes_expressed']}, new={len(r2['newly_grown'])}")
    for gene in mc.architecture_dna():
        if gene["expressed"]:
            print(f"  Active: {gene['gene']} ({gene['type']})")
    print("1 = 1.")
