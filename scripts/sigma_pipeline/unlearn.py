# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""Python mirror of ``src/sigma/pipeline/unlearn.{h,c}``."""

from __future__ import annotations

import dataclasses
import math
from typing import List, Sequence, Tuple


EPS = 1e-8


def fnv1a_64(s: str) -> int:
    h = 0xcbf29ce484222325
    for b in s.encode("utf-8"):
        h ^= b
        h = (h * 0x100000001b3) & 0xffffffffffffffff
    return h if h != 0 else 1


def verify(weights: Sequence[float], target: Sequence[float]) -> float:
    if not weights or not target or len(weights) != len(target):
        return 1.0
    dot = 0.0
    nw = 0.0
    nt = 0.0
    for wi, ti in zip(weights, target):
        dot += wi * ti
        nw  += wi * wi
        nt  += ti * ti
    if nw <= EPS or nt <= EPS:
        return 1.0
    cs = dot / (math.sqrt(nw) * math.sqrt(nt))
    cs = min(1.0, max(-1.0, cs))
    sigma = 1.0 - abs(cs)
    if sigma < 0.0: sigma = 0.0
    if sigma > 1.0: sigma = 1.0
    return sigma


def apply(weights: List[float], target: Sequence[float],
          strength: float) -> float:
    """Projection removal: w ← w − strength · proj_t(w)."""
    if not weights or not target or len(weights) != len(target):
        return 0.0
    strength = max(0.0, min(1.0, strength))
    dot = sum(wi * ti for wi, ti in zip(weights, target))
    tt  = sum(ti * ti for ti in target)
    if tt <= EPS:
        return 0.0
    coeff = strength * dot / tt
    delta_l1 = 0.0
    for i, ti in enumerate(target):
        d = coeff * ti
        weights[i] -= d
        delta_l1 += abs(d)
    return delta_l1


@dataclasses.dataclass
class UnlearnRequest:
    subject_hash: int
    strength: float
    max_iters: int
    sigma_target: float


@dataclasses.dataclass
class UnlearnResult:
    subject_hash: int
    n_iters: int
    sigma_before: float
    sigma_after: float
    l1_shrunk: float
    succeeded: bool


def iterate(weights: List[float], target: Sequence[float],
            req: UnlearnRequest) -> UnlearnResult:
    if not weights or not target or len(weights) != len(target):
        raise ValueError("weights/target mismatch")
    if req.strength <= 0 or req.max_iters <= 0:
        raise ValueError("strength/max_iters must be positive")
    if not (0 < req.sigma_target <= 1):
        raise ValueError("sigma_target must be in (0, 1]")

    sigma_before = verify(weights, target)
    strength = max(0.0, min(1.0, req.strength))
    total_l1 = 0.0
    iters = 0
    for _ in range(req.max_iters):
        sig = verify(weights, target)
        if sig >= req.sigma_target:
            break
        total_l1 += apply(weights, target, strength)
        iters += 1
    sigma_after = verify(weights, target)
    return UnlearnResult(
        subject_hash=req.subject_hash,
        n_iters=iters,
        sigma_before=sigma_before,
        sigma_after=sigma_after,
        l1_shrunk=total_l1,
        succeeded=sigma_after >= req.sigma_target,
    )
