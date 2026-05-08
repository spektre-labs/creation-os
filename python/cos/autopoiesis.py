# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Autopoiesis-inspired **lab harness**: operational closure + structural coupling scored by σ.

This module is a **pedagogical** mapping (Maturana / Varela–style narratives) onto
:class:`~cos.sigma_gate.SigmaGate` — it is **not** biology, **not** a proof of life-like agency,
and **not** “AGI achieved”. Ω-loop, dream, evolve, etc. are named in prose as **related**
Creation OS subsystems; this file stays a small, testable σ-threshold story. See
``docs/CLAIM_DISCIPLINE.md`` and paper #86 for theory vs implementation scope.
"""
from __future__ import annotations

from typing import Any, Callable, Dict

from cos.sigma_gate import SigmaGate

__all__ = ["Autopoietic"]


def _verdict_str(verdict: Any) -> str:
    return str(getattr(verdict, "name", verdict))


def _is_abstain(verdict: Any) -> bool:
    return "ABSTAIN" in _verdict_str(verdict).upper()


def _is_accept(verdict: Any) -> bool:
    return "ACCEPT" in _verdict_str(verdict).upper()


def _is_rethink(verdict: Any) -> bool:
    return "RETHINK" in _verdict_str(verdict).upper()


class Autopoietic:
    """Discrete σ-gated produce / maintain / couple cycle (integration metaphor)."""

    def __init__(self, gate: Any = None) -> None:
        self.gate = gate or SigmaGate()
        self.components: Dict[str, Dict[str, Any]] = {}
        self.boundary_σ: float = 0.5
        self.alive: bool = True
        self.generation: int = 0

    def produce(self, component_name: str, producer_fn: Callable[[], Any]) -> Dict[str, Any]:
        """Register a component if the gate does not ABSTAIN on the produce proposal."""
        result = producer_fn()
        sigma, verdict = self.gate.score(f"produce {component_name}", str(result))
        s = float(sigma)
        if not _is_abstain(verdict):
            self.components[str(component_name)] = {
                "value": result,
                "σ": s,
                "sigma": s,
                "generation": self.generation,
            }
            self._update_boundary()
            return {"produced": True, "σ": round(s, 4), "verdict": _verdict_str(verdict)}
        return {
            "produced": False,
            "σ": round(s, 4),
            "sigma": round(s, 4),
            "reason": "σ too high — component rejected",
            "verdict": _verdict_str(verdict),
        }

    def maintain(self) -> Dict[str, Any]:
        """Re-score degraded components (σ > 0.7); improve σ or drop the entry."""
        repaired = 0
        for name, comp in list(self.components.items()):
            if float(comp["σ"]) <= 0.7:
                continue
            sigma_new, _verdict = self.gate.score(f"repair {name}", str(comp["value"]))
            sn = float(sigma_new)
            if sn < float(comp["σ"]):
                comp["σ"] = sn
                comp["sigma"] = sn
                repaired += 1
            else:
                del self.components[name]
        self.generation += 1
        self._update_boundary()
        return {
            "repaired": repaired,
            "generation": self.generation,
            "boundary_σ": round(self.boundary_σ, 4),
        }

    def couple(self, external_input: Any) -> Dict[str, Any]:
        """Environmental input scored against current components (structural coupling)."""
        sigma, verdict = self.gate.score(str(self.components), str(external_input))
        s = float(sigma)
        vs = _verdict_str(verdict)
        if _is_accept(verdict):
            return {
                "coupled": True,
                "σ": round(s, 4),
                "sigma": round(s, 4),
                "identity_preserved": True,
                "verdict": vs,
            }
        if _is_rethink(verdict):
            return {
                "coupled": True,
                "σ": round(s, 4),
                "sigma": round(s, 4),
                "identity_preserved": True,
                "warning": "boundary stressed",
                "verdict": vs,
            }
        return {
            "coupled": False,
            "σ": round(s, 4),
            "sigma": round(s, 4),
            "identity_preserved": True,
            "reason": "rejected — would dissolve boundary",
            "verdict": vs,
        }

    def is_alive(self) -> Dict[str, Any]:
        """Lab predicate: non-empty component map and aggregate boundary σ below threshold."""
        has_components = len(self.components) > 0
        boundary_intact = float(self.boundary_σ) < 0.8
        self.alive = bool(has_components and boundary_intact)
        return {
            "alive": self.alive,
            "components": len(self.components),
            "boundary_σ": round(float(self.boundary_σ), 4),
            "generation": self.generation,
        }

    def _update_boundary(self) -> None:
        """Aggregate boundary readout = mean component σ (empty → 1.0)."""
        if not self.components:
            self.boundary_σ = 1.0
            return
        vals = [float(c["σ"]) for c in self.components.values()]
        self.boundary_σ = sum(vals) / len(vals)
