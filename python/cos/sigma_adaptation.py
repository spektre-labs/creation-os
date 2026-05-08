# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
σ-adaptation history for **test-time training** lab (`cos ttt --track`).

**Lab only:** rolling window statistics; not a calibrated online-learning metric.
"""
from __future__ import annotations

from cos.sigma_gate import SigmaGate  # noqa: F401 — σ kernel anchor (sigma_gate.h / Python lite)

from typing import Any, Dict, List


class SigmaAdaptationTracker:
    """Append-only history of σ before/after gated TTT steps."""

    def __init__(self) -> None:
        self.history: List[Dict[str, Any]] = []

    def track(self, step: int, sigma_before: float, sigma_after: float, tag: str) -> None:
        sb, sa = float(sigma_before), float(sigma_after)
        row: Dict[str, Any] = {
            "step": int(step),
            "sigma_before": sb,
            "sigma_after": sa,
            "delta": round(sa - sb, 8),
            "tag": str(tag),
        }
        self.history.append(row)

    def last_n(self, n: int) -> List[Dict[str, Any]]:
        if n <= 0:
            return []
        return list(self.history[-n:])

    def learning_efficiency(self) -> float:
        """
        Higher is worse Drift upward (positive ``delta``) counts against efficiency.
        Returns average ``-delta`` over RETHINK rows (σ drop ⇒ positive score).
        """
        rows = [x for x in self.history if str(x.get("tag", "")).startswith("RETHINK")]
        if not rows:
            return 0.0
        return float(sum(-float(x.get("delta", 0.0)) for x in rows)) / float(len(rows))

    def should_continue_learning(self, *, window: int = 10) -> bool:
        """Heuristic: continue if recent RETHINK rows still show σ trending up on average."""
        recent = self.last_n(window)
        rethink = [x for x in recent if str(x.get("tag", "")).startswith("RETHINK")]
        if len(rethink) < 2:
            return True
        mean_delta = sum(float(x.get("delta", 0.0)) for x in rethink) / float(len(rethink))
        return mean_delta > -0.01

    def should_consolidate(
        self, *, window: int = 8, min_negative_steps: int = 3, delta_threshold: float = -0.02
    ) -> bool:
        """
        Lab hook for σ-consolidation: recent tracked steps show σ trending down (improvement).

        Requires ``min_negative_steps`` rows in the last ``window`` with delta below
        ``delta_threshold``. Used by :class:`cos.sigma_consolidation.SigmaConsolidation`.
        """
        recent = self.last_n(window)
        if len(recent) < min_negative_steps:
            return False
        thr = float(delta_threshold)
        neg = sum(1 for x in recent if float(x.get("delta", 0.0)) < thr)
        return neg >= int(min_negative_steps)


__all__ = ["SigmaAdaptationTracker"]
