# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Φ-proxy via σ **dynamics** — a computable lab surrogate, not IIT’s Φ.

Empirical work in living neuronal cultures reports that a **proxy for integrated
information** correlates **positively** with **Bayesian surprise** (complexity)
under a variational FEP-style decomposition; see Mayama *et al.*,
`arXiv:2510.04084 <https://arxiv.org/abs/2510.04084>`_. Those results motivate a
**pedagogical** bridge: rapid belief updating ↔ larger |Δσ| in a σ trace.

**This module does not compute IIT Φ (NP-hard / exponentially costly in general).**
It exposes **mean |Δσ|** over a short window and a toy **parts-vs-whole** gap.
That is **not** “consciousness measured”, **not** a substitute for the paper’s
Φ_R proxy construction, and **not** AGI achieved — see ``docs/CLAIM_DISCIPLINE.md``.
"""
from __future__ import annotations

from typing import Any, Dict, List, Sequence

from cos.sigma_gate import SigmaGate

__all__ = ["PhiProxy"]


class PhiProxy:
    """Track σ samples and summarize **change-rate** and **coarse** “integration” proxies."""

    def __init__(self, gate: Any = None) -> None:
        self.gate = gate or SigmaGate()
        self.sigma_trace: List[float] = []

    def record(self, sigma: float) -> None:
        self.sigma_trace.append(float(sigma))

    def phi(self, window: int = 10) -> float:
        """Φ-proxy = mean |Δσ| over the last ``window`` samples (minimum two points)."""
        if len(self.sigma_trace) < 2:
            return 0.0
        recent = self.sigma_trace[-max(2, int(window)) :]
        diffs = [abs(recent[i + 1] - recent[i]) for i in range(len(recent) - 1)]
        return round(sum(diffs) / len(diffs), 4) if diffs else 0.0

    def learning_phase(self) -> str:
        """Heuristic phase label from (Φ-proxy, recent mean σ) — hill-shaped story, lab only."""
        phi = self.phi()
        n = min(len(self.sigma_trace), 10)
        avg_σ = (
            sum(self.sigma_trace[-n:]) / n
            if self.sigma_trace
            else 0.5
        )

        if phi > 0.1 and avg_σ > 0.5:
            return "exploration"
        if phi > 0.05 and avg_σ < 0.5:
            return "insight"
        if phi < 0.05 and avg_σ < 0.2:
            return "mastery"
        if phi < 0.02 and avg_σ > 0.5:
            return "stuck"
        return "transitioning"

    def integration(self, subsystem_sigma_values: Sequence[float]) -> Dict[str, Any]:
        """Parts vs whole: if last system σ is below mean(parts), flag coarse 'integration'."""
        if not subsystem_sigma_values:
            return {"integrated": False, "phi_proxy": 0.0}

        avg_parts = sum(float(x) for x in subsystem_sigma_values) / len(subsystem_sigma_values)
        system_σ = float(self.sigma_trace[-1]) if self.sigma_trace else 0.5
        phi = max(0.0, float(avg_parts) - system_σ)

        return {
            "integrated": phi > 0.05,
            "phi_proxy": round(phi, 4),
            "system_σ": round(system_σ, 4),
            "parts_avg_σ": round(avg_parts, 4),
            "interpretation": (
                "whole > sum of parts" if phi > 0.05 else "no integration detected"
            ),
        }
