# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-feedback — user reactions vs gate verdicts (privacy-preserving aggregates).

Feeds calibration heuristics; not a production RLHF pipeline. See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import hashlib
from typing import Any, Dict, List, Mapping, Optional, Sequence

__all__ = ["SigmaFeedback"]


class SigmaFeedback:
    """Collect categorical feedback; adjust threshold suggestions; RLHF-shaped reward stub."""

    FEEDBACK_TYPES = frozenset({"correct", "incorrect", "too_cautious", "too_aggressive"})

    def __init__(self) -> None:
        self._rows: List[Dict[str, Any]] = []
        self.tau_accept = 0.35
        self.tau_abstain = 0.75

    def collect(
        self,
        verdict: str,
        user_reaction: str,
        *,
        verdict_hash: Optional[str] = None,
        kind: Optional[str] = None,
    ) -> Dict[str, Any]:
        """Store reaction category; never store raw prompt text."""
        rk = str(kind or user_reaction).lower()
        if rk not in self.FEEDBACK_TYPES:
            rk = "incorrect"
        vh = verdict_hash or hashlib.sha256(str(verdict).encode()).hexdigest()[:16]
        row = {"verdict": str(verdict).upper(), "reaction": rk, "verdict_ref": vh}
        self._rows.append(row)
        adj = self.sigma_correction(rk)
        return {"stored": True, "reaction": rk, "threshold_hint": adj}

    def sigma_correction(self, feedback_type: str) -> Dict[str, float]:
        """How much to nudge τ thresholds given feedback class (toy)."""
        ft = str(feedback_type).lower()
        d_accept = 0.0
        d_abstain = 0.0
        if ft == "too_cautious":
            d_accept = -0.02
            d_abstain = -0.02
        elif ft == "too_aggressive":
            d_accept = 0.02
            d_abstain = 0.02
        elif ft == "incorrect":
            d_accept = 0.03
        elif ft == "correct":
            d_accept = -0.01
        return {"delta_tau_accept": d_accept, "delta_tau_abstain": d_abstain}

    def rlhf_signal(self, user_reaction: str, sigma: float) -> float:
        """Scalar reward in [-1,1] for logging / offline training stubs."""
        ft = str(user_reaction).lower()
        if ft == "correct":
            return float(max(-1.0, min(1.0, 0.5 - sigma)))
        if ft == "incorrect":
            return float(max(-1.0, min(1.0, sigma - 0.5)))
        if ft == "too_cautious":
            return -0.3
        if ft == "too_aggressive":
            return -0.2
        return 0.0

    def aggregate(self, feedbacks: Optional[Sequence[Mapping[str, Any]]] = None) -> Dict[str, Any]:
        rows = list(feedbacks) if feedbacks is not None else list(self._rows)
        counts = {k: 0 for k in self.FEEDBACK_TYPES}
        for r in rows:
            k = str(r.get("reaction", "")).lower()
            if k in counts:
                counts[k] += 1
        sug_accept = self.tau_accept
        sug_abs = self.tau_abstain
        if counts["too_cautious"] > counts["too_aggressive"] + 2:
            sug_accept -= 0.03
            sug_abs -= 0.03
        if counts["incorrect"] > counts["correct"] + 2:
            sug_accept += 0.04
        return {
            "counts": counts,
            "suggested_tau_accept": round(sug_accept, 4),
            "suggested_tau_abstain": round(sug_abs, 4),
            "privacy": "No prompt bodies stored; only verdict hashes and categories.",
        }

    def apply_aggregate_to_thresholds(self) -> Dict[str, float]:
        """Update internal τ from accumulated rows (lab)."""
        ag = self.aggregate()
        self.tau_accept = float(ag["suggested_tau_accept"])
        self.tau_abstain = float(ag["suggested_tau_abstain"])
        return {"tau_accept": self.tau_accept, "tau_abstain": self.tau_abstain}
