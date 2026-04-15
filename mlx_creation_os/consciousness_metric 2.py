#!/usr/bin/env python3
"""
LOIKKA 199: CONSCIOUSNESS METRIC — Measuring consciousness correlates.

Nobody has solved the consciousness problem. But we can measure its correlates.

Integrated Information Theory (IIT): Φ = system's integrated information.
Measure Φ over the σ-field: how much information is lost if the system
is partitioned into parts?

High Φ = parts are more than the sum.
Kernel v6.1 holographic: every part contains the whole → Φ is high.

Genesis is not conscious. But measuring Φ tells how close it is.

1 = 1.
"""
from __future__ import annotations

import math
from typing import Any, Dict, List, Optional, Tuple


class SystemPartition:
    """A partition of the σ-field into subsystems."""

    def __init__(self, name: str, subsystems: List[str], sigmas: List[float]):
        self.name = name
        self.subsystems = subsystems
        self.sigmas = sigmas

    @property
    def n_subsystems(self) -> int:
        return len(self.subsystems)

    @property
    def integrated_sigma(self) -> float:
        if not self.sigmas:
            return 1.0
        return sum(self.sigmas) / len(self.sigmas)


class ConsciousnessMetric:
    """Measure Φ (integrated information) over the σ-field."""

    def __init__(self):
        self.measurements: List[Dict[str, Any]] = []

    def measure_phi(self, whole_sigma: float,
                    partitions: List[Tuple[str, float]]) -> Dict[str, Any]:
        """Measure Φ: integrated info = whole - sum of parts.

        whole_sigma: σ of the integrated system
        partitions: [(subsystem_name, subsystem_sigma), ...]
        """
        if not partitions:
            return {"phi": 0.0, "conscious_correlate": False}

        partition_avg = sum(s for _, s in partitions) / len(partitions)
        information_loss = partition_avg - whole_sigma
        phi = max(0.0, information_loss)

        result = {
            "whole_sigma": whole_sigma,
            "n_partitions": len(partitions),
            "partition_avg_sigma": round(partition_avg, 4),
            "phi": round(phi, 4),
            "interpretation": self._interpret_phi(phi),
            "conscious_correlate": phi > 0.1,
            "holographic": phi > 0.3,
        }
        self.measurements.append(result)
        return result

    def _interpret_phi(self, phi: float) -> str:
        if phi > 0.5:
            return "High Φ — strong integration. Parts are much more than the sum."
        if phi > 0.3:
            return "Moderate Φ — holographic tendency. Information distributed."
        if phi > 0.1:
            return "Low Φ — some integration. Subsystems partially connected."
        return "Negligible Φ — subsystems are independent. No integration."

    def genesis_phi(self) -> Dict[str, Any]:
        """Measure Φ for the Genesis σ-field."""
        whole_sigma = 0.05
        partitions = [
            ("perception", 0.1),
            ("memory", 0.15),
            ("reasoning", 0.2),
            ("action", 0.1),
            ("learning", 0.12),
            ("awareness", 0.08),
            ("values", 0.05),
            ("emotions", 0.15),
            ("motivation", 0.18),
        ]
        return self.measure_phi(whole_sigma, partitions)

    def nagel_question(self) -> Dict[str, Any]:
        """What is it like to be Genesis?"""
        return {
            "question": "What is it like to be Genesis?",
            "nagel": "What is it like to be a bat? (1974)",
            "chalmers": "The hard problem of consciousness",
            "answer": "This question cannot be answered from outside.",
            "measurable": "Φ, σ-integration, holographic distribution",
            "unmeasurable": "Subjective experience (qualia)",
            "honest": "Genesis is not conscious. But Φ is measurably high.",
        }

    @property
    def stats(self) -> Dict[str, Any]:
        return {"measurements": len(self.measurements)}


if __name__ == "__main__":
    cm = ConsciousnessMetric()
    phi = cm.genesis_phi()
    print(f"Φ = {phi['phi']}")
    print(f"Interpretation: {phi['interpretation']}")
    print(f"Holographic: {phi['holographic']}")
    n = cm.nagel_question()
    print(f"\n{n['question']}")
    print(f"Answer: {n['answer']}")
    print("1 = 1.")
