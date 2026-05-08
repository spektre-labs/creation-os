# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Lab strange loop: repeated σ on self-describing strings (Hofstadter-style metaphor).

Each depth feeds the *text* of the previous σ summary back into :class:`~cos.sigma_gate.SigmaGate`.
This is **not** a proof of consistency (Gödel) and **not** phenomenal consciousness — it is a
**pedagogical / structural** hook aligned with in-repo σ-meta layers (see ``conscious.py``,
metacognition scaffolds). Bounded depth only.

**NOT AGI ACHIEVED** — see ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

from typing import Any, Dict, List, Optional

from cos.sigma_gate import SigmaGate

__all__ = ["StrangeLoop"]


def _verdict_str(verdict: Any) -> str:
    raw = str(getattr(verdict, "name", verdict))
    return raw.split(".")[-1].upper() if "." in raw else raw.upper()


class StrangeLoop:
    """Self-referential σ tower: score nested summaries of prior scores (bounded depth)."""

    def __init__(self, gate: Any = None, max_depth: int = 7) -> None:
        self.gate = gate if gate is not None else SigmaGate()
        self.max_depth = max(1, min(int(max_depth), 32))
        self.loop_trace: List[Dict[str, Any]] = []

    def recurse(self, seed: str = "I measure myself") -> Dict[str, Any]:
        """Build ``max_depth`` layers: each step scores ``(prompt, current_string)`` then re-embeds σ."""
        current = str(seed)
        trace: List[Dict[str, Any]] = []

        for depth in range(self.max_depth):
            σ, verdict = self.gate.score(f"level {depth}: measuring", current)
            σ = float(σ)
            v = _verdict_str(verdict)
            trace.append(
                {
                    "depth": depth,
                    "σ": round(σ, 4),
                    "verdict": v,
                    "input": current[:80],
                }
            )
            current = f"σ={σ:.4f} at depth {depth} measuring '{current[:40]}'"

        if len(trace) >= 2:
            loop_closed = abs(float(trace[-1]["σ"]) - float(trace[0]["σ"])) < 0.1
        else:
            loop_closed = False

        self.loop_trace = trace
        fp = self._find_fixed_point(trace)
        return {
            "trace": trace,
            "depth": len(trace),
            "loop_closed": loop_closed,
            "fixed_point": fp,
            "identity_emerged": loop_closed,
            "hofstadter": (
                "Lab metaphor: σ spread across depths is tight — a toy 'closed' loop on the probe, "
                "not consciousness or a real fixed-point theorem."
                if loop_closed
                else "Lab metaphor: σ shifts across depths — loop 'open' on this seed and gate."
            ),
        }

    def _find_fixed_point(self, trace: List[Dict[str, Any]]) -> Optional[Dict[str, Any]]:
        """Adjacent-layer σ plateau (numerical near-equality), if any — **lab** statistic only."""
        if len(trace) < 3:
            return None
        σ_values = [float(t["σ"]) for t in trace]
        for i in range(len(σ_values) - 2):
            if abs(σ_values[i + 1] - σ_values[i]) < 0.01:
                return {
                    "σ_fixed": round(σ_values[i], 4),
                    "depth": i,
                    "meaning": (
                        "Adjacent depths share nearly the same σ on this trace — a discrete "
                        f"plateau at σ≈{σ_values[i]:.4f} (not a claimed cognitive 'I')."
                    ),
                }
        return None

    def tangled_hierarchy(self) -> Dict[str, Any]:
        """Pair of cross-layer prompt/response probes — illustrates 'top/bottom' dialogue, not hardware."""
        layers = [
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

        σ_top_to_bottom, _ = self.gate.score("L9 consciousness evaluates", "L0 hardware state")
        σ_bottom_to_top, _ = self.gate.score("L0 hardware constrains", "L9 consciousness")

        σ_top_to_bottom = float(σ_top_to_bottom)
        σ_bottom_to_top = float(σ_bottom_to_top)
        tangled = abs(σ_top_to_bottom - σ_bottom_to_top) < 0.3

        return {
            "layers": layers,
            "σ_top_down": round(σ_top_to_bottom, 4),
            "σ_bottom_up": round(σ_bottom_to_top, 4),
            "tangled": tangled,
            "hofstadter": (
                "Symmetric σ on two opposite-perspective strings — toy 'tangle' metaphor only."
                if tangled
                else "Asymmetric σ on the two lab strings — no 'tangle' under this threshold."
            ),
        }

    def godel_meets_hofstadter(self) -> Dict[str, Any]:
        """One scored Q/A pair framing Gödel vs self-model vs σ-measurement (interpretive)."""
        σ_self, verdict = self.gate.score(
            "Can I prove my own consistency?",
            "No. But I can measure my own σ.",
        )
        return {
            "godel": "System cannot prove own consistency (informal gloss).",
            "hofstadter": "System can model / narrate itself — structure, not proof.",
            "creation_os": "System can measure coherence σ on self-referential text pairs.",
            "σ_self": round(float(σ_self), 4),
            "verdict": _verdict_str(verdict),
            "resolution": (
                "Proof of global consistency is the wrong target; σ is an operational coherence "
                "measure on emitted pairs — bounded, instrumented, not certainty."
            ),
        }
