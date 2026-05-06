# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-gradient as a drive signal: curiosity vs anxiety from coherence dynamics."""

from __future__ import annotations

from typing import Any, Dict, List


class SigmaDrive:
    """Internal drive from recent σ only (no external reward)."""

    def __init__(self) -> None:
        self.sigma_history: List[float] = []
        self.emotion_log: List[Dict[str, Any]] = []

    def record(self, sigma: float) -> None:
        self.sigma_history.append(float(sigma))

    def gradient(self, window: int = 5) -> float:
        """Finite-difference trend: (last − first) / span over the last ``window`` points."""
        if len(self.sigma_history) < 2:
            return 0.0
        w = max(1, int(window))
        recent = self.sigma_history[-w:]
        if len(recent) < 2:
            return 0.0
        span = len(recent)
        return float((recent[-1] - recent[0]) / span)

    def emotion(self) -> Dict[str, Any]:
        if len(self.sigma_history) < 2:
            return {"state": "neutral", "σ": 0.0, "gradient": 0.0}

        sigma = float(self.sigma_history[-1])
        grad = self.gradient()

        if grad < -0.02:
            state = "curiosity"
        elif grad > 0.02:
            state = "anxiety"
        elif sigma < 0.1:
            state = "satisfaction"
        elif sigma > 0.5 and abs(grad) < 0.01:
            state = "frustration"
        else:
            state = "neutral"

        result = {"state": state, "σ": round(sigma, 4), "gradient": round(grad, 6)}
        self.emotion_log.append(result)
        return result

    def should_continue(self) -> bool:
        e = self.emotion()
        st = e["state"]
        if st == "curiosity":
            return True
        if st == "satisfaction":
            return False
        if st == "frustration":
            return False
        if st == "anxiety":
            return False
        return True
