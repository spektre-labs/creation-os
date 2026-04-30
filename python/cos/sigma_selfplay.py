# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
σ-**self-play**: one model proposes two stochastic draws, then cross-critiques; a σ probe
scores answers and critiques so the harness can prefer **stable** answers that are **hard
to attack** (lab scoring — not a formal proof system).

See ``docs/CLAIM_DISCIPLINE.md``.
"""
from __future__ import annotations

from typing import Any, Tuple


def _call_generate(model: Any, question: str, temperature: float) -> str:
    gen = getattr(model, "generate", None)
    if gen is None or not callable(gen):
        raise TypeError("model must expose generate(question, ...) -> str")
    try:
        return str(gen(question, temperature=temperature)).strip()
    except TypeError:
        return str(gen(question)).strip()


def _call_critique(model: Any, question: str, answer: str) -> str:
    crit = getattr(model, "critique", None)
    if crit is None or not callable(crit):
        raise TypeError("model must expose critique(question, answer) -> str")
    return str(crit(question, answer)).strip()


class SigmaSelfPlay:
    """Single-deputy self-play with σ-scored answers and counter-critiques."""

    def __init__(self, gate: Any) -> None:
        self.gate = gate

    def play(self, model: Any, question: str) -> Tuple[str, float]:
        """
        Score ``(1 - σ_answer) + σ_critique_of_other`` — prefer confident answers whose
        rival critique looks strained (higher σ on the critique of the *other* answer).
        """
        q = (question or "").strip()
        if not q:
            return "", 0.5

        a1 = _call_generate(model, q, 0.3)
        a2 = _call_generate(model, q, 0.9)
        s1 = max(0.0, min(1.0, float(self.gate.compute_sigma(model, q, a1))))
        s2 = max(0.0, min(1.0, float(self.gate.compute_sigma(model, q, a2))))

        c1 = _call_critique(model, q, a1)
        c2 = _call_critique(model, q, a2)
        sc1 = max(0.0, min(1.0, float(self.gate.compute_sigma(model, q, c1))))
        sc2 = max(0.0, min(1.0, float(self.gate.compute_sigma(model, q, c2))))

        score_1 = (1.0 - s1) + sc2
        score_2 = (1.0 - s2) + sc1
        if score_1 > score_2:
            return a1, s1
        if score_2 > score_1:
            return a2, s2
        return (a1 if s1 <= s2 else a2), min(s1, s2)


__all__ = ["SigmaSelfPlay"]
