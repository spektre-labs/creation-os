# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Shared σ-evaluation metrics (stdlib-only AUROC / smECE / SNR helpers)."""
from __future__ import annotations

import math
from typing import Iterable, List, Sequence, Tuple

__all__ = [
    "auroc_binary",
    "accuracy_answered",
    "sm_ece_binary",
    "snr_sigma_separation",
    "safe_mean",
    "safe_stdev",
]


def safe_mean(xs: Sequence[float]) -> float:
    if not xs:
        return 0.0
    return float(sum(xs) / len(xs))


def safe_stdev(xs: Sequence[float]) -> float:
    if len(xs) < 2:
        return 0.0
    m = safe_mean(xs)
    v = sum((x - m) ** 2 for x in xs) / (len(xs) - 1)
    return float(math.sqrt(max(0.0, v)))


def auroc_binary(y_true: Sequence[int], scores: Sequence[float]) -> float:
    """Binary AUROC; ``y_true[i]`` ∈ {0,1}; higher ``scores[i]`` means more likely class 1."""
    if len(y_true) != len(scores) or not y_true:
        return 0.5
    pairs: List[Tuple[float, int]] = sorted(zip(scores, y_true), key=lambda t: t[0])
    n_pos = sum(y_true)
    n_neg = len(y_true) - n_pos
    if n_pos == 0 or n_neg == 0:
        return 0.5
    rank_sum = 0.0
    i = 0
    n = len(pairs)
    while i < n:
        j = i
        score = pairs[i][0]
        while j < n and pairs[j][0] == score:
            j += 1
        ranks = sum(range(i + 1, j + 1)) / (j - i)
        for k in range(i, j):
            if pairs[k][1] == 1:
                rank_sum += ranks
        i = j
    auc = (rank_sum - n_pos * (n_pos + 1) / 2) / (n_pos * n_neg)
    return float(max(0.0, min(1.0, auc)))


def accuracy_answered(
    y_true: Sequence[int],
    y_pred_answered: Sequence[int],
    answered_mask: Sequence[bool],
) -> float:
    """Accuracy over items where the gate did not abstain (``answered_mask``)."""
    hits = 0
    tot = 0
    for yt, yp, ans in zip(y_true, y_pred_answered, answered_mask):
        if not ans:
            continue
        tot += 1
        if int(yt) == int(yp):
            hits += 1
    return float(hits / tot) if tot else 0.0


def sm_ece_binary(y_true: Sequence[int], probs: Sequence[float], *, n_bins: int = 10) -> float:
    """Simplified maximum ECE: max over bins of |accuracy − mean confidence|."""
    if len(y_true) != len(probs) or not y_true:
        return 0.0
    bins = max(3, min(20, int(n_bins)))
    worst = 0.0
    for b in range(bins):
        lo = b / bins
        hi = (b + 1) / bins
        accs: List[float] = []
        confs: List[float] = []
        for yt, p in zip(y_true, probs):
            p = float(max(0.0, min(1.0, p)))
            if b < bins - 1:
                in_bin = lo <= p < hi
            else:
                in_bin = p >= lo
            if in_bin:
                accs.append(float(yt))
                confs.append(p)
        if not accs:
            continue
        gap = abs(safe_mean(accs) - safe_mean(confs))
        worst = max(worst, gap)
    return float(worst)


def snr_sigma_separation(y_error: Sequence[int], sigmas: Sequence[float]) -> float:
    """Separation SNR: |E[σ|error] − E[σ|¬error]| / (std(σ)+ε). ``y_error`` 1 = incorrect."""
    if len(y_error) != len(sigmas) or not y_error:
        return 0.0
    s_err = [float(s) for e, s in zip(y_error, sigmas) if int(e) == 1]
    s_ok = [float(s) for e, s in zip(y_error, sigmas) if int(e) == 0]
    if not s_err or not s_ok:
        return 0.0
    diff = abs(safe_mean(s_err) - safe_mean(s_ok))
    denom = safe_stdev([float(x) for x in sigmas]) + 1e-6
    return float(diff / denom)
