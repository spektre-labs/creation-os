# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
σ-**debate**: two deputies exchange structured arguments; a shared **σ** probe scores each
turn for coherence / strain (lower is treated as stronger in this lab harness).

**Not** a human-jury debate stack and **not** RLHF — inject real ``argue`` / ``compute_sigma``
implementations for research. See ``docs/CLAIM_DISCIPLINE.md``.
"""
from __future__ import annotations

from statistics import mean
from typing import Any, Dict, List, Optional, Tuple


class SigmaDebate:
    """Two models argue; the gate scores each reply; lowest mean σ wins (or ``TIE``)."""

    def __init__(self, model_a: Any, model_b: Any, gate: Any) -> None:
        self.model_a = model_a
        self.model_b = model_b
        self.gate = gate

    def debate(self, question: str, *, rounds: int = 3) -> Tuple[Optional[str], str, float]:
        """
        Run up to ``rounds`` alternating turns. Return ``(winner_text, side, stat)`` where
        ``side`` is ``\"A\"``, ``\"B\"``, or ``\"TIE\"``; on tie ``winner_text`` is ``None``
        and ``stat`` is the average of both sides' mean σ.
        """
        q = (question or "").strip()
        if not q:
            return None, "TIE", 0.5

        history_a: List[Dict[str, Any]] = []
        history_b: List[Dict[str, Any]] = []
        rmax = max(1, int(rounds))

        for _r in range(rmax):
            arg_a = str(self.model_a.argue(q, history_b)).strip()
            sigma_a = float(self.gate.compute_sigma(self.model_a, q, arg_a))
            sigma_a = max(0.0, min(1.0, sigma_a))
            history_a.append({"argument": arg_a, "sigma": sigma_a})

            arg_b = str(self.model_b.argue(q, history_a)).strip()
            sigma_b = float(self.gate.compute_sigma(self.model_b, q, arg_b))
            sigma_b = max(0.0, min(1.0, sigma_b))
            history_b.append({"argument": arg_b, "sigma": sigma_b})

        avg_a = float(mean(h["sigma"] for h in history_a)) if history_a else 0.5
        avg_b = float(mean(h["sigma"] for h in history_b)) if history_b else 0.5

        if avg_a < avg_b:
            return history_a[-1]["argument"], "A", avg_a
        if avg_b < avg_a:
            return history_b[-1]["argument"], "B", avg_b
        return None, "TIE", (avg_a + avg_b) / 2.0


__all__ = ["SigmaDebate"]
