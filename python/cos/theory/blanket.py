# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-gate framed as a **Markov-blanket-shaped boundary** over Creation OS layers (lab metaphor).

:class:`~cos.sigma_gate.SigmaGate` scores **pairwise coherence**; this module uses those scores as
a **scalar “boundary integrity” readout** for tracing nested L0–L9 style slices. That is a
**pedagogical alignment** with hierarchical inference/boundary narratives (e.g. Friston 2018
literature), **not** a formal proof that the gate is a literal Markov blanket and **not** an AGI
claim. See ``docs/CLAIM_DISCIPLINE.md`` for lab vs theory scope.
"""
from __future__ import annotations

from typing import Any, Dict, List, Optional

from cos.sigma_gate import SigmaGate

__all__ = ["MarkovBlanket", "NestedBlankets"]


def _verdict_str(verdict: Any) -> str:
    return str(getattr(verdict, "name", verdict))


def _is_abstain(verdict: Any) -> bool:
    return "ABSTAIN" in _verdict_str(verdict).upper()


class MarkovBlanket:
    """Sensory / active / internal slots with σ scored via the shared :class:`SigmaGate`."""

    def __init__(self, name: str, gate: Any = None) -> None:
        self.name = str(name)
        self.gate = gate or SigmaGate()
        self.internal: Dict[str, Any] = {}
        self.sensory: Dict[str, Any] = {}
        self.active: Dict[str, Any] = {}
        self.children: List["MarkovBlanket"] = []
        self.sigma_boundary: float = 0.5

    def sense(self, external_input: Any) -> Dict[str, Any]:
        """Inbound boundary: score internal vs external (distortion / mismatch proxy)."""
        sigma, verdict = self.gate.score(str(self.internal), str(external_input))
        s = float(sigma)
        self.sensory["last"] = external_input
        self.sensory["σ"] = s
        self.sensory["sigma"] = s
        self.sigma_boundary = s
        vs = _verdict_str(verdict)
        return {"σ": round(s, 4), "sigma": round(s, 4), "verdict": vs}

    def act(self, output: Any) -> Dict[str, Any]:
        """Outbound boundary: score internal vs proposed output (coherence proxy)."""
        sigma, verdict = self.gate.score(str(self.internal), str(output))
        s = float(sigma)
        self.active["last"] = output
        self.active["σ"] = s
        self.active["sigma"] = s
        # Blend sensory and active readouts for the blanket’s single integrity scalar.
        if "σ" in self.sensory:
            self.sigma_boundary = (float(self.sensory["σ"]) + s) / 2.0
        else:
            self.sigma_boundary = s
        vs = _verdict_str(verdict)
        return {"σ": round(s, 4), "sigma": round(s, 4), "verdict": vs}

    def update_internal(self, new_state: Dict[str, Any]) -> None:
        """Merge into internal/hidden state bucket."""
        self.internal.update(new_state)

    def add_child(self, child_blanket: "MarkovBlanket") -> None:
        """Nest a finer-grained blanket (Lk ⊂ Lk+1 chain in :class:`NestedBlankets`)."""
        self.children.append(child_blanket)

    def boundary_integrity(self) -> float:
        """Mean σ over this blanket and nested children (lower ≈ tighter boundary in this lab)."""
        values: List[float] = [float(self.sigma_boundary)]
        for child in self.children:
            values.append(float(child.boundary_integrity()))
        return round(sum(values) / max(len(values), 1), 4)

    def is_autonomous(self) -> bool:
        """Lab heuristic: “autonomous” ⇔ aggregate boundary σ stays below threshold."""
        return self.boundary_integrity() < 0.3


class NestedBlankets:
    """Ten canonical bands (L0–L9) linked as a linear child chain for demos and tracing."""

    LAYERS = [
        "L0_HARDWARE",
        "L1_INFERENCE",
        "L2_COGNITION",
        "L3_MEMORY",
        "L4_AGENCY",
        "L5_LEARNING",
        "L6_PROTOCOL",
        "L7_SAFETY",
        "L8_DEPLOYMENT",
        "L9_CONSCIOUS",
    ]

    def __init__(self, gate: Any = None) -> None:
        self.gate = gate or SigmaGate()
        self.blankets: Dict[str, MarkovBlanket] = {}
        self._build()

    def _build(self) -> None:
        prev: Optional[MarkovBlanket] = None
        for layer in self.LAYERS:
            b = MarkovBlanket(layer, self.gate)
            self.blankets[layer] = b
            if prev is not None:
                prev.add_child(b)
            prev = b

    def propagate(self, input_data: Any) -> Dict[str, Any]:
        """Walk L0→L9: sense → internal update → act; stop if any layer returns ABSTAIN."""
        signal = input_data
        trace: List[Dict[str, Any]] = []
        for layer in self.LAYERS:
            b = self.blankets[layer]
            sense_result = b.sense(signal)
            b.update_internal({"processed": str(signal)[:100]})
            act_result = b.act(signal)
            trace.append(
                {
                    "layer": layer,
                    "σ_sense": sense_result["σ"],
                    "σ_act": act_result["σ"],
                    "verdict": sense_result["verdict"],
                }
            )
            if _is_abstain(sense_result["verdict"]) or _is_abstain(act_result["verdict"]):
                trace.append({"layer": layer, "BLOCKED": True})
                break
        sense_rows = [t for t in trace if "σ_sense" in t]
        total_σ = (
            round(sum(t["σ_sense"] for t in sense_rows) / max(len(sense_rows), 1), 4)
            if sense_rows
            else 0.5
        )
        return {
            "trace": trace,
            "layers_passed": len(trace),
            "total_σ": total_σ,
        }

    def system_integrity(self) -> Dict[str, Any]:
        """Aggregate integrity from the L0 root blanket (includes nested means)."""
        root = self.blankets["L0_HARDWARE"]
        integrity = root.boundary_integrity()
        return {
            "integrity": integrity,
            "autonomous": integrity < 0.3,
            "per_layer": {layer: b.boundary_integrity() for layer, b in self.blankets.items()},
        }
