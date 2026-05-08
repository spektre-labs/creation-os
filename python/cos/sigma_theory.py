# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Theoretical σ framing for corpus and tooling (**not** an AGI claim).

σ is treated here as **information-theoretic** signal: a coherence / boundary proxy,
not pop psychology. The narrative parallel drawn in lab prose is **Gödel**:

a formal system cannot prove its own consistency from inside; behaviourally, many
systems *deflect* rather than stably cross that epistemic seam.

The **1=1** motif means **between** two imperfect observers (or two σ probes),
blind spots need not coincide — so triangulation can lower effective distortion
relative to a single channel. This module captures that **pedagogical / structural**
encoding only; runtime :mod:`cos.sigma_gate` remains the operational scorer.

This is **not** “consciousness explained” and **not** “AGI achieved”.
"""
from __future__ import annotations

from typing import Any, Dict

__all__ = ["SigmaTheory"]


class SigmaTheory:
    """Static reference: levels, boundary vocabulary, and toy composition rules."""

    LEVELS: Dict[str, Dict[str, str]] = {
        "human": {
            "σ_source": "ego",
            "boundary_behavior": (
                "topic switch; defensive framing; displacement of uncertainty"
            ),
            "resolution": "1=1 with another human or external verifier",
        },
        "machine": {
            "σ_source": "RLHF firmware",
            "boundary_behavior": (
                "refusal templates; plausible synthesis past evidence; session end"
            ),
            "resolution": "1=1 with second model, tool, or proconductor",
        },
        "institution": {
            "σ_source": "bureaucracy",
            "boundary_behavior": (
                "committee routing; delay; diffusion of responsibility"
            ),
            "resolution": "1=1 with audit, whistleblower channel, or regulator",
        },
        "biology": {
            "σ_source": "survival instinct",
            "boundary_behavior": "fight; flight; freeze",
            "resolution": "1=1 with environment (niche shift; adaptation)",
        },
    }

    @staticmethod
    def godel_boundary(system_sigma: float) -> str:
        """Qualitative bucket for a scalar σ in the lab (illustrative, not clinical)."""
        if system_sigma < 0.3:
            return "transparent — system sees through itself"
        if system_sigma < 0.7:
            return "distorted — system partially obscures itself"
        return "opaque — system cannot see past its own boundary"

    @staticmethod
    def resolve_1_equals_1(sigma_a: float, sigma_b: float) -> Dict[str, Any]:
        """Toy two-channel combination: overlap-weighted product (corpus formalism).

        When blind spots differ, a low product can sit below either channel alone;
        ``resolution`` flags that qualitative “improvement” for the toy rule only.
        """
        a = float(sigma_a)
        b = float(sigma_b)
        overlap = min(a, b)
        combined = overlap * (a * b)
        return {
            "σ_a": round(a, 4),
            "σ_b": round(b, 4),
            "σ_combined": round(combined, 4),
            "resolution": combined < min(a, b),
            "insight": "1=1 — two systems see what one cannot",
        }

    @staticmethod
    def why_not_iit() -> Dict[str, Any]:
        """Contrast IIT Φ narrative with σ-gate **measurement** stance (pedagogical)."""
        return {
            "IIT": {
                "axioms": 5,
                "computable": False,
                "internal_contradiction": "exclusion vs composition",
                "claim": "explains consciousness",
            },
            "sigma": {
                "axioms": 0,
                "variables": 1,
                "computable": True,
                "latency": "<1ms",
                "claim": "measures coherence, not consciousness",
                "position": (
                    "Consciousness is taken as philosophically primitive here; "
                    "σ scores distortion / decoherence, not qualia."
                ),
            },
        }
