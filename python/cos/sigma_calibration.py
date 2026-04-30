# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Binary calibration helpers (ECE, threshold search) for σ-gate harnesses."""
from __future__ import annotations

from typing import Tuple

import numpy as np


def ece_binary_equal_mass(
    y: np.ndarray,
    p: np.ndarray,
    *,
    n_bins: int = 10,
) -> float:
    """
    Expected calibration error with **equal-mass** bins on ``p`` (predicted positive prob).
    ``y`` ∈ {0,1}. Returns a scalar in [0,1].
    """
    y = np.asarray(y, dtype=np.int64).ravel()
    p = np.asarray(p, dtype=np.float64).ravel()
    if y.size == 0 or p.size == 0 or y.size != p.size:
        return float("nan")
    n_bins = max(3, int(n_bins))
    order = np.argsort(p)
    cuts = np.array_split(order, n_bins)
    ece = 0.0
    n = float(y.size)
    for idx in cuts:
        if idx.size == 0:
            continue
        yy = y[idx]
        pp = p[idx]
        acc = float(np.mean(yy))
        conf = float(np.mean(pp))
        ece += (idx.size / n) * abs(acc - conf)
    return float(ece)


def best_threshold_precision(
    y: np.ndarray,
    scores: np.ndarray,
    *,
    grid: int = 101,
) -> Tuple[float, float]:
    """
    Scan thresholds on ``scores`` to maximize precision at positive prediction.

    Returns ``(threshold, precision)``; precision is nan if no positives predicted at best t.
    """
    y = np.asarray(y, dtype=np.int64).ravel()
    s = np.asarray(scores, dtype=np.float64).ravel()
    if y.size == 0 or s.size == 0 or y.size != s.size:
        return float("nan"), float("nan")
    lo = float(np.min(s))
    hi = float(np.max(s))
    if not np.isfinite(lo) or not np.isfinite(hi) or lo >= hi:
        t = float(np.median(s))
        pred = (s >= t).astype(np.int64)
        tp = float(np.sum((pred == 1) & (y == 1)))
        fp = float(np.sum((pred == 1) & (y == 0)))
        prec = tp / (tp + fp) if (tp + fp) > 0 else float("nan")
        return t, prec
    best_t = lo
    best_p = -1.0
    for i in range(int(grid)):
        t = lo + (hi - lo) * (i / max(1, grid - 1))
        pred = (s >= t).astype(np.int64)
        tp = float(np.sum((pred == 1) & (y == 1)))
        fp = float(np.sum((pred == 1) & (y == 0)))
        if tp + fp <= 0:
            continue
        prec = tp / (tp + fp)
        if prec > best_p:
            best_p = prec
            best_t = t
    if best_p < 0:
        return float("nan"), float("nan")
    return float(best_t), float(best_p)


__all__ = ["best_threshold_precision", "ece_binary_equal_mass"]
