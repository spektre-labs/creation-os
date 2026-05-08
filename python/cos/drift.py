# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Semantic drift on σ: compare current batches to a baseline via Welch-style separation and Gaussian KL."""
from __future__ import annotations

from cos.sigma_gate import SigmaGate  # noqa: F401 — σ kernel anchor (sigma_gate.h / Python lite)

import math
from typing import Any, Dict, List, Optional, Sequence, Union

__all__ = ["SigmaDrift"]

ScalarSeq = Sequence[Union[int, float]]


class SigmaDrift:
    """Detect when the σ distribution shifts away from a stored baseline."""

    def __init__(self) -> None:
        self.baseline: Optional[Dict[str, Any]] = None

    def set_baseline(self, σ_values: ScalarSeq) -> None:
        """Set reference σ distribution."""
        xs = [float(x) for x in σ_values]
        n = len(xs)
        if n == 0:
            self.baseline = None
            return
        mean = sum(xs) / n
        var = sum((x - mean) ** 2 for x in xs) / n
        std = math.sqrt(var)
        self.baseline = {
            "mean": mean,
            "std": std,
            "n": n,
            "values": list(xs),
        }

    def detect(self, current_σ_values: ScalarSeq) -> Dict[str, Any]:
        """Compare current σ values to the baseline (Welch-style t / SE + Gaussian KL)."""
        cur = [float(x) for x in current_σ_values]
        if not cur:
            return {"drift": 0.0, "alert": False, "reason": "no current samples"}
        if not self.baseline:
            return {"drift": 0.0, "alert": False, "reason": "no baseline"}

        curr_mean = sum(cur) / len(cur)
        curr_var = sum((x - curr_mean) ** 2 for x in cur) / len(cur)
        curr_std = math.sqrt(curr_var)

        n0 = int(self.baseline["n"])
        n1 = len(cur)
        v0 = float(self.baseline["std"]) ** 2
        v1 = curr_std**2
        se = math.sqrt(v0 / max(n0, 1) + v1 / max(n1, 1))
        mean_diff = abs(curr_mean - float(self.baseline["mean"]))
        if se < 1e-10:
            t_stat = 0.0 if mean_diff < 1e-10 else 10.0
        else:
            t_stat = mean_diff / se

        drift = min(1.0, t_stat / 5.0)

        s0 = float(self.baseline["std"])
        s1 = curr_std
        if s0 > 1e-10 and s1 > 1e-10:
            m0 = float(self.baseline["mean"])
            kl = (
                math.log(s1 / s0)
                + (s0**2 + (m0 - curr_mean) ** 2) / (2 * s1**2)
                - 0.5
            )
            kl = max(0.0, float(kl))
        else:
            kl = 0.0

        alert = drift > 0.3 or kl > 1.0

        return {
            "drift": round(drift, 4),
            "kl_divergence": round(kl, 4),
            "baseline_mean": round(float(self.baseline["mean"]), 4),
            "current_mean": round(curr_mean, 4),
            "alert": alert,
            "reason": "σ distribution shifted" if alert else "stable",
        }
