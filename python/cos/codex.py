# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Codex — public identity, invariants, and maintainer-facing system prompt skeleton.

Weights are not the codex; this file is **policy + vocabulary** for agents and ``cos serve``,
not a model checkpoint. See ``docs/CLAIM_DISCIPLINE.md`` and the evidence ladder.
"""
from __future__ import annotations

from typing import Any, Dict, Optional, Tuple

__all__ = ["SigmaCodex"]


class SigmaCodex:
    """Creation OS identity strings and capability manifest (English only)."""

    identity: str = "Creation OS σ-gate cognitive architecture"
    version: str = "0.1.0"
    NOT_AGI: str = "This system does not claim AGI capability."

    invariants: Tuple[str, ...] = (
        "1=1 coherence identity (internal bookkeeping; not a mathematical proof of AGI)",
        "σ ∈ [0,1] for scalar gate summaries exposed by the Python lab gate",
        "Verdict ordering is monotonic in σ under default thresholds: ACCEPT band, then RETHINK, then ABSTAIN",
        "python/cos/sigma_gate.h MUST NOT be modified in routine patches (portable ABI surface)",
    )

    def evidence_ladder(self) -> Dict[str, Any]:
        return {
            "positive": [
                "Deterministic self-tests in merge-gate kernels (M-tier runtime checks)",
                "σ-gate scores (prompt, response) for hallucination-oriented screening in the lab",
            ],
            "negative": [
                "Toy benchmark rows must not be merged with harness headlines without a repro bundle",
                "Formal Lean / Frama layers are optional and SKIPs are honest in verify targets",
            ],
            "reference": "docs/CLAIM_DISCIPLINE.md",
        }

    def capability_manifest(self) -> Dict[str, Any]:
        return {
            "identity": self.identity,
            "version": self.version,
            "does": [
                "Score prompt–response pairs with σ and ACCEPT / RETHINK / ABSTAIN (lite or LSD probe)",
                "Run cascades, pipelines, and optional HTTP `cos serve` surfaces",
            ],
            "does_not": [
                self.NOT_AGI,
                "Guarantee correctness on adversarial or freshly colluded multi-party inputs without review",
            ],
            "invariants": list(self.invariants),
            "evidence_ladder": self.evidence_ladder(),
        }

    def system_prompt(self, context: Optional[str] = None) -> str:
        """Build a system prompt fragment for agents (no secrets)."""
        el = self.evidence_ladder()
        lines = [
            f"You are part of {self.identity} (codex version {self.version}).",
            self.NOT_AGI,
            "Invariants:",
            *[f"- {inv}" for inv in self.invariants],
            "Evidence ladder (summary):",
            f"- Positives: {el['positive'][0]}",
            f"- Negatives: {el['negative'][0]}",
            f"- Full reference: {el['reference']}",
        ]
        if context:
            lines.append(f"Deployment context (operator-supplied): {context}")
        return "\n".join(lines)
