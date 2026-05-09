# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""**Category-theoretic** wording around :class:`~cos.sigma_gate.SigmaGate` (lab metaphor).

Real category theory pins down **formal naturality**, composition, and functors. This module is
**not** a complete categorical model of Creation OS, **not** a proof of functor laws from σ alone,
and **not** a replacement for real semantics — objects and morphisms are **Python handles**,
``map_*`` updates tables, and σ / verdict are **compatibility scores** on strings. Treat
**isomorphism** and **composition preserved** as **illustrative thresholds**, not theorems. **Zero**
extra dependencies. **Not AGI achieved.** See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

from typing import Any, Dict, Sequence

from cos.sigma_gate import SigmaGate

__all__ = ["Object", "Morphism", "SigmaFunctor"]


class Object:
    """Vertex label with an optional payload string."""

    def __init__(self, name: str, state: Any = None) -> None:
        self.name = str(name)
        self.state = state if state is not None else self.name

    def __repr__(self) -> str:
        return f"Obj({self.name!r})"


class Morphism:
    """Arrow with a numeric **σ** (distortion) on how much structure is kept."""

    def __init__(
        self,
        source: Object,
        target: Object,
        name: str = "",
        *,
        sigma: float = 0.0,
    ) -> None:
        self.source = source
        self.target = target
        self.name = str(name) if name else f"{source.name}→{target.name}"
        self.sigma = float(sigma)

    @property
    def σ(self) -> float:  # noqa: PLC2401 — public API for σ-as-morphism norm
        return self.sigma

    def is_isomorphism(self) -> bool:
        """Toy **1=1**: σ below a small cutoff."""
        return self.sigma < 0.1

    def __repr__(self) -> str:
        return f"{self.name!r}(σ={self.sigma:.3f})"


class SigmaFunctor:
    """Table-driven image of declared objects/morphisms under string scoring."""

    def __init__(self, gate: Any = None) -> None:
        self.gate = gate or SigmaGate()
        self.object_map: Dict[str, Dict[str, Any]] = {}
        self.morphism_map: Dict[str, Dict[str, Any]] = {}

    def map_object(self, declared_obj: Object) -> Dict[str, Any]:
        σ, _verdict = self.gate.score("declare", str(declared_obj.state))
        sigma = float(σ)
        realized = Object(f"R({declared_obj.name})", state=f"realized({declared_obj.state})")
        entry = {
            "declared": declared_obj,
            "realized": realized,
            "σ": round(sigma, 4),
            "sigma": round(sigma, 4),
            "isomorphic": sigma < 0.1,
        }
        self.object_map[declared_obj.name] = entry
        return dict(entry)

    def map_morphism(self, morphism: Morphism) -> Dict[str, Any]:
        left = f"transform {morphism.source.name}→{morphism.target.name}"
        σ, _v = self.gate.score(left, str(morphism.target.state))
        sigma = float(σ)
        realized_morphism = Morphism(
            source=Object(f"R({morphism.source.name})"),
            target=Object(f"R({morphism.target.name})"),
            name=f"R({morphism.name})",
            sigma=sigma,
        )
        entry = {
            "declared": morphism,
            "realized": realized_morphism,
            "σ": round(sigma, 4),
            "sigma": round(sigma, 4),
            "structure_preserved": sigma < 0.2,
        }
        self.morphism_map[morphism.name] = entry
        return dict(entry)

    def is_faithful(self) -> bool:
        """Toy injectivity: distinct morphisms get distinct σ at two decimal places."""
        if not self.morphism_map:
            return True
        σ_values = [float(m["σ"]) for m in self.morphism_map.values()]
        unique = len({round(s, 2) for s in σ_values})
        return unique == len(σ_values)

    def composition_check(self, f: Morphism, g: Morphism) -> Dict[str, Any]:
        """Compare composed vs part scores (string sketch, not a proof of functoriality)."""
        σ_composed, _vc = self.gate.score(
            f"compose {f.name} then {g.name}",
            f"{f.target.state} → {g.target.state}",
        )
        σ_f, _vf = self.gate.score("part", str(f.target.state))
        σ_g, _vg = self.gate.score("part", str(g.target.state))

        s_c = float(σ_composed)
        s_parts = (float(σ_f) + float(σ_g)) / 2.0
        composition_error = abs(s_c - s_parts)

        return {
            "σ_composed": round(s_c, 4),
            "sigma_composed": round(s_c, 4),
            "σ_parts": round(s_parts, 4),
            "sigma_parts": round(s_parts, 4),
            "composition_preserved": composition_error < 0.2,
            "error": round(composition_error, 4),
        }

    def natural_transformation_sigma(self, sigma_trajectory: Sequence[float]) -> Dict[str, Any]:
        """Toy naturality of a σ trace: penalize large single-step jumps."""
        traj = [float(x) for x in sigma_trajectory]
        if len(traj) < 2:
            return {"natural": True, "smooth": True, "max_jump": 0.0, "avg_change": 0.0}
        jumps = [abs(traj[i + 1] - traj[i]) for i in range(len(traj) - 1)]
        max_jump = max(jumps)
        avg_jump = sum(jumps) / len(jumps)
        return {
            "natural": max_jump < 0.3,
            "max_jump": round(max_jump, 4),
            "avg_change": round(avg_jump, 4),
            "smooth": avg_jump < 0.05,
        }

    def natural_transformation_σ(self, σ_trajectory: Sequence[float]) -> Dict[str, Any]:  # noqa: PLC2401
        return self.natural_transformation_sigma(σ_trajectory)
