# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
σ-governed **reasoning depth**: extend inference only while the interrupt sees σ as
non-increasing (no positive step in the Q16 ``d_sigma`` sense); bail on ABSTAIN or when
σ rises again after the first step (overthinking / instability).

**Lab scaffold:** inject ``generate_thought``, ``compute_sigma``, and ``generate_answer``.
This is not a faithful reproduction of any commercial ``o*`` stack; see
``docs/CLAIM_DISCIPLINE.md`` for headline hygiene.
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import Callable, List, Optional, Sequence, Tuple

from .sigma_gate_core import SigmaState, Verdict, sigma_gate, sigma_update

Thought = str


@dataclass
class SigmaReasoning:
    """
    Reasoning loop gated by ``sigma_gate_core`` (same Q16 semantics as the 12-byte path).

    Policy sketch:
      * ``ACCEPT`` → emit final answer from accumulated thoughts.
      * ``ABSTAIN`` → ``None`` (fail closed).
      * ``RETHINK`` with ``depth > 0`` and ``d_sigma > 0`` → σ rose again → stop and answer
        (overthinking guard).
      * Else append thought and continue until ``max_depth``.
    """

    generate_thought: Callable[[str, Sequence[Thought]], Thought]
    compute_sigma: Callable[[Thought], float]
    generate_answer: Callable[[str, Sequence[Thought]], Thought]
    k_raw: float = 0.92

    def reason(self, prompt: str, max_depth: int = 10) -> Optional[Thought]:
        state = SigmaState()
        thoughts: List[Thought] = []

        for depth in range(int(max_depth)):
            thought = self.generate_thought(prompt, tuple(thoughts))
            probe_sigma = max(0.0, min(1.0, float(self.compute_sigma(thought))))
            kr = max(0.0, min(1.0, float(self.k_raw)))
            sigma_update(state, probe_sigma, kr)
            verdict = sigma_gate(state)

            if verdict == Verdict.ACCEPT:
                return self.generate_answer(prompt, tuple(thoughts))

            if verdict == Verdict.ABSTAIN:
                return None

            if verdict == Verdict.RETHINK and int(state.d_sigma) > 0 and depth > 0:
                return self.generate_answer(prompt, tuple(thoughts))

            thoughts.append(thought)

        return None


__all__ = ["SigmaReasoning", "Thought"]
