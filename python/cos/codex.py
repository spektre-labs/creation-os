# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Codex — public identity, invariants, and maintainer-facing system prompt skeleton.

Weights are not the codex; this file is **policy + vocabulary** for agents and ``cos serve``,
not a model checkpoint. See ``docs/CLAIM_DISCIPLINE.md`` and the evidence ladder.
"""
from __future__ import annotations

from typing import Any, Dict, List, Optional, Tuple

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

    def __init__(self) -> None:
        self.rules: Dict[str, Any] = {
            "threshold_nudge": 0.02,
            "prompt_strictness": 0.35,
            "routing_depth_bias": 0,
        }

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

    def evolve_codex(
        self,
        eval_data: List[Any],
        gate: Any,
        *,
        max_iterations: int = 10,
        evolver: Optional[Any] = None,
    ) -> Dict[str, Any]:
        """GEPA-style seed loop: adapt codex rules from scored rows until σ mean plateaus.

        Wire ``gate`` (or your model wrapper) so it consults :attr:`rules` if you want rule-driven σ;
        the loop is lab-only and does not claim autonomous self-improvement.
        """
        from cos.evolve import SigmaAdapter, SigmaEvolve

        ev = evolver if evolver is not None else SigmaEvolve(gate=gate)
        adapter = SigmaAdapter()
        current = dict(self.rules)
        history: List[Dict[str, Any]] = []
        prev_mean: Optional[float] = None

        for i in range(max(1, int(max_iterations))):
            results = adapter.evaluate(gate, eval_data)
            if not results:
                break
            mean_sigma = sum(float(r["σ"]) for r in results) / float(len(results))
            if prev_mean is not None and mean_sigma > prev_mean - 1e-9:
                break
            traces = adapter.extract_traces(results)
            history.append({"iter": i, "mean_σ": round(mean_sigma, 6), "rules": dict(current)})
            current = ev.mutate_reflective(gate, eval_data, traces, base_rules=current)
            self.rules.update(current)
            prev_mean = mean_sigma

        return {
            "rules": dict(self.rules),
            "history": history,
            "iterations": len(history),
            "final_mean_σ": history[-1]["mean_σ"] if history else None,
        }
