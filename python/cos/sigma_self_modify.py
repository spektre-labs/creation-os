# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
σ-governed **self-modification scaffold** (lab): filter code mutations **before** expensive
execution using the same cognitive interrupt as runtime inference
(``sigma_gate_core`` / ``sigma_gate.h``).

**Stable-kernel thesis (orientation):** ``sigma_gate.h`` is treated as an **invariant**
in this scaffold — any mutation touching it (or license / SPDX governance files) maps
to **ABSTAIN** so it is never passed to ``run_mutation``. This is **not** a formal proof
that your executor cannot be corrupted; wire your own sandbox + tests. See
``docs/CLAIM_DISCIPLINE.md``.
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Callable, Dict, Iterable, List, Optional, Sequence, Tuple

from .sigma_gate_core import SigmaState, Verdict, sigma_gate, sigma_update


@dataclass
class MutationOutcome:
    """Result of executing one mutation (operator-supplied)."""

    score: float
    sigma_after: Optional[float] = None


DEFAULT_INVARIANT_FRAGMENTS: Tuple[str, ...] = (
    "sigma_gate.h",
    "sigma_sampler.h",
    "LICENSE",
    "SPDX-License-Identifier",
    "AGENTS.md",
    "codex",
)


class SigmaSelfModifier:
    """
    Darwin-style mutate → evaluate loop with a **σ preflight** on each mutation string.

    - **ABSTAIN** (preflight): skip ``run_mutation`` entirely.
    - **RETHINK** / **ACCEPT**: call ``run_mutation``; for **RETHINK**, optionally reject
      if ``sigma_after`` exceeds ``tau_sigma_post``.
    """

    def __init__(
        self,
        *,
        invariant_fragments: Sequence[str] = DEFAULT_INVARIANT_FRAGMENTS,
        max_chars_rethink: int = 50_000,
        tau_sigma_post: float = 0.95,
    ):
        self._invariants = tuple(str(x) for x in invariant_fragments)
        self._max_chars_rethink = max(1024, int(max_chars_rethink))
        self.tau_sigma_post = float(tau_sigma_post)

    def _mutation_risk_01(self, mutation: str) -> float:
        """Heuristic risk in [0,1]; high → more likely ABSTAIN/RETHINK after ``sigma_gate``."""
        blob = mutation or ""
        low = blob.lower()
        for frag in self._invariants:
            if frag.lower() in low:
                return 1.0
        if len(blob) > self._max_chars_rethink:
            return 0.58
        return 0.12

    def evaluate_mutation_sigma(self, mutation: str) -> Tuple[SigmaState, Verdict]:
        """
        Map mutation text to Q16 state via ``sigma_update`` + ``sigma_gate`` (same kernel
        as inference policy).
        """
        st = SigmaState()
        r = self._mutation_risk_01(mutation)
        k_raw = max(0.0, min(1.0, 1.0 - r))
        sigma_update(st, r, k_raw)
        return st, sigma_gate(st)

    def filter_mutations(
        self, mutations: Iterable[str]
    ) -> List[Tuple[str, Verdict, SigmaState]]:
        out: List[Tuple[str, Verdict, SigmaState]] = []
        for m in mutations:
            st, v = self.evaluate_mutation_sigma(m)
            out.append((m, v, st))
        return out

    def evolve(
        self,
        agent_code: str,
        benchmark: Any,
        *,
        generate_mutations: Callable[[str], Iterable[str]],
        run_mutation: Callable[[str, Any], MutationOutcome],
        best_baseline_score: float = float("-inf"),
    ) -> Tuple[Optional[str], float, List[Dict[str, Any]]]:
        """
        Iterate mutations; skip ABSTAIN preflight; run the rest; keep highest ``score``.

        Returns ``(best_mutation_or_none, best_score, trace)`` where ``trace`` is a JSON-safe
        list of dicts for observability.
        """
        best: Optional[str] = None
        best_score = float(best_baseline_score)
        trace: List[Dict[str, Any]] = []

        for mut in generate_mutations(agent_code):
            st, v = self.evaluate_mutation_sigma(mut)
            prev = len(trace)
            trace.append(
                {
                    "verdict_preflight": v.name,
                    "k_eff_q16": int(st.k_eff),
                    "d_sigma_q16": int(st.d_sigma),
                    "preview": (mut or "")[:120],
                }
            )
            if v == Verdict.ABSTAIN:
                continue

            out = run_mutation(mut, benchmark)
            if v == Verdict.RETHINK and out.sigma_after is not None:
                if float(out.sigma_after) > self.tau_sigma_post:
                    trace[prev]["skipped_post_sigma"] = True
                    continue

            if float(out.score) > best_score:
                best_score = float(out.score)
                best = mut
                trace[prev]["accepted_score"] = best_score

        return best, best_score, trace


__all__ = [
    "DEFAULT_INVARIANT_FRAGMENTS",
    "MutationOutcome",
    "SigmaSelfModifier",
]
