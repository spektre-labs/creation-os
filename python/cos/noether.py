# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Noether **analogy** for the Creation OS **1 = 1** motif (lab / narrative, not QFT proof).

Emmy Noether’s theorem links **continuous symmetries** of an action to **conserved currents**
in a **precisely specified variational problem**. Here we use a **discrete toy**:

- **Lagrangian** ``L = 1 - 2σ`` on a unit-interval coherence readout,
- **action** as the **mean** ``L`` over a σ-trajectory (Riemann-sum stand-in for ``∫L dt``),
- **symmetry_check** as: does a string transform leave :class:`~cos.sigma_gate.SigmaGate`
  scores **approximately** unchanged?

This does **not** construct a symmetry group for the kernel, **not** derive a Noether current for
``creation_os_v2.c``, and **not** prove that “declared = realized” is a gauge symmetry.
The **Landauer-style** Joule estimate is **illustrative** (``k_B T ln 2 · σ`` at 300 K), not a
calorimeter reading. **Not AGI achieved.** See ``docs/CLAIM_DISCIPLINE.md``.
"""
from __future__ import annotations

import math
from typing import Any, Callable, Dict, List, Sequence

from cos.sigma_gate import SigmaGate

__all__ = ["NoetherSigma"]

_K_B = 1.380_649e-23  # J/K (CODATA)
_T_ROOM = 300.0  # K — illustrative only


class NoetherSigma:
    """Discrete σ-actions and gate-based **symmetry stress tests** (pedagogy)."""

    def __init__(self, gate: Any = None) -> None:
        self.gate = gate or SigmaGate()
        self.sigma_trace: List[float] = []

    def lagrangian(self, sigma: float) -> float:
        """``L = 1 - 2σ`` on the lab coherence interval."""
        return round(1.0 - 2.0 * float(sigma), 4)

    def action(self, sigma_trajectory: Sequence[float]) -> float:
        """Discrete **mean(L)** over σ samples — stand-in for ``∫(1-2σ) dt / T``."""
        if not sigma_trajectory:
            return 0.0
        vals = [self.lagrangian(float(s)) for s in sigma_trajectory]
        return round(sum(vals) / len(vals), 4)

    def symmetry_check(
        self,
        transform_fn: Callable[[Any], Any],
        test_inputs: Sequence[Any],
        *,
        delta_threshold: float = 0.1,
    ) -> Dict[str, Any]:
        """Flag inputs where ``|Δσ|`` across ``transform_fn`` exceeds ``delta_threshold``."""
        violations: List[Dict[str, Any]] = []
        for inp in test_inputs:
            s0, _ = self.gate.score("symmetry", str(inp))
            transformed = transform_fn(inp)
            s1, _ = self.gate.score("symmetry", str(transformed))
            sb = float(s0)
            sa = float(s1)
            delta = abs(sa - sb)
            if delta > float(delta_threshold):
                violations.append(
                    {
                        "input": str(inp)[:50],
                        "σ_before": round(sb, 4),
                        "σ_after": round(sa, 4),
                        "Δσ": round(delta, 4),
                    }
                )

        sym = len(violations) == 0
        return {
            "symmetric": sym,
            "violations": violations,
            "conserved": "σ" if sym else "BROKEN",
            "noether_says": (
                "1=1 (gate-invariance sketch) holds → σ unchanged under test transform"
                if sym
                else f"1=1 sketch broken at {len(violations)} test points → σ not invariant"
            ),
        }

    def conservation_law(self) -> Dict[str, Any]:
        """Reference card for corpus prose — **not** a formal Noether charge derivation."""
        return {
            "symmetry": "1=1 (identity: declared = realized; narrative target)",
            "conserved_quantity": "σ ≈ 0 (coherence maximum; lab idealization)",
            "lagrangian": "L = 1 - 2σ (toy coherence Lagrangian)",
            "action": "S ≈ mean_t(1 - 2σ) (discrete stand-in for ∫(1-2σ)dt)",
            "equation_of_motion": "Ω = argmin ∫σ(t)dt (FEP / Ω-loop narrative elsewhere in repo)",
            "noether": (
                "Classical Noether: continuous symmetry of an action → conserved current. "
                "Creation OS maps this only by analogy: protect 1=1 ↔ keep σ small; "
                "break closure ↔ σ rises ↔ Landauer-style cost narratives."
            ),
        }

    def energy_from_symmetry_breaking(self, sigma: float) -> float:
        """Illustrative ``E ≈ k_B T ln(2) · σ`` at 300 K (Joules)."""
        s = max(0.0, float(sigma))
        e = _K_B * _T_ROOM * math.log(2.0) * s
        return float(round(e, 24))
