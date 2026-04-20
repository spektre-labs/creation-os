# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""Python mirror of ``src/sigma/pipeline/diagnostic.{h,c}``."""

from __future__ import annotations

import dataclasses
import math
from typing import List, Sequence, Tuple


TOPK = 3


def _clamp01(x: float) -> float:
    if math.isnan(x):
        return 1.0
    return max(0.0, min(1.0, x))


def _softmax(lp: Sequence[float]) -> List[float]:
    finite = [x for x in lp if math.isfinite(x)]
    if not finite:
        raise ValueError("no finite logprobs")
    mx = max(finite)
    out: List[float] = []
    s = 0.0
    for x in lp:
        if not math.isfinite(x):
            out.append(0.0)
            continue
        e = math.exp(x - mx)
        out.append(e)
        s += e
    if s <= 0:
        raise ValueError("softmax denominator is zero")
    return [v / s for v in out]


@dataclasses.dataclass
class Diagnostic:
    sigma: float
    factor_entropy: float
    factor_gap: float
    factor_maxprob: float
    entropy: float
    effective_k: int
    max_prob: float
    gap_top12: float
    top: List[Tuple[int, float]]      # length TOPK
    cf_target: float
    cf_sigma: float


def _empty(cf_target: float) -> Diagnostic:
    return Diagnostic(
        sigma=1.0, factor_entropy=1.0, factor_gap=1.0, factor_maxprob=1.0,
        entropy=0.0, effective_k=0, max_prob=0.0, gap_top12=0.0,
        top=[(-1, 0.0)] * TOPK,
        cf_target=_clamp01(cf_target if cf_target != 0 else 0.90) or 0.90,
        cf_sigma=1.0,
    )


def _counterfactual_sigma(p: Sequence[float], top1: int,
                          target: float, ln_n: float) -> float:
    n = len(p)
    if n <= 0 or top1 < 0 or top1 >= n:
        return 1.0
    target = max(0.0001, min(0.9999, target))
    rest_old = 1.0 - p[top1]
    rest_new = 1.0 - target
    scale = (rest_new / rest_old) if rest_old > 0 else 0.0

    H = 0.0
    top2_new = 0.0
    for i, pi in enumerate(p):
        q = target if i == top1 else pi * scale
        if q > 0:
            H -= q * math.log(q)
        if i != top1 and q > top2_new:
            top2_new = q
    fact_entropy = max(0.0, min(1.0, H / ln_n if ln_n > 0 else 0.0))
    fact_gap     = _clamp01(1.0 - (target - top2_new))
    fact_maxprob = _clamp01(1.0 - target)
    return _clamp01((fact_entropy + fact_gap + fact_maxprob) / 3.0)


def explain(logprobs: Sequence[float], cf_target: float = 0.90) -> Diagnostic:
    if not logprobs:
        return _empty(cf_target)
    n = len(logprobs)
    if n > 65536:
        return _empty(cf_target)
    try:
        p = _softmax(logprobs)
    except ValueError:
        return _empty(cf_target)

    H = 0.0
    for v in p:
        if v > 0:
            H -= v * math.log(v)
    ln_n = math.log(n)

    sorted_idx = sorted(range(n), key=lambda i: p[i], reverse=True)
    top: List[Tuple[int, float]] = []
    for i in range(TOPK):
        if i < len(sorted_idx):
            j = sorted_idx[i]
            top.append((j, p[j]))
        else:
            top.append((-1, 0.0))

    max_prob   = top[0][1]
    second     = top[1][1] if len(top) > 1 else 0.0
    gap_top12  = max(0.0, max_prob - second)

    factor_entropy = _clamp01(H / ln_n if ln_n > 0 else 0.0)
    factor_gap     = _clamp01(1.0 - gap_top12)
    factor_maxprob = _clamp01(1.0 - max_prob)
    sigma          = _clamp01((factor_entropy + factor_gap + factor_maxprob) / 3.0)

    cf = cf_target if 0 < cf_target < 1 else 0.90
    cf_sigma = _counterfactual_sigma(p, top[0][0], cf, ln_n)

    return Diagnostic(
        sigma=sigma,
        factor_entropy=factor_entropy,
        factor_gap=factor_gap,
        factor_maxprob=factor_maxprob,
        entropy=H,
        effective_k=int(round(math.exp(H))),
        max_prob=max_prob,
        gap_top12=gap_top12,
        top=top,
        cf_target=cf,
        cf_sigma=cf_sigma,
    )
