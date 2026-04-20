# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""Python mirror of ``src/sigma/pipeline/moe.{h,c}``.

Pure Python; no numpy.  Byte-for-byte decision parity with the C
primitive on the canonical sweep (verified by the parity test).
"""

from __future__ import annotations

import dataclasses
import enum
import math
from typing import List, Optional, Sequence


class Width(enum.Enum):
    Q  = (0, 0.25, "Q")
    H  = (1, 0.50, "H")
    TQ = (2, 0.75, "TQ")
    F  = (3, 1.00, "F")

    @property
    def frac(self) -> float:
        return self.value[1]

    @property
    def label(self) -> str:
        return self.value[2]


@dataclasses.dataclass
class Activation:
    expert_id: int
    width: Width
    score: float

    @property
    def width_frac(self) -> float:
        return self.width.frac


@dataclasses.dataclass(frozen=True)
class Cutoffs:
    c_quarter: float = 0.15
    c_half: float = 0.30
    c_three_quarter: float = 0.50


DEFAULT_CUTOFFS = Cutoffs()


def _pick_width(sigma: float, c: Cutoffs = DEFAULT_CUTOFFS) -> Width:
    try:
        s = float(sigma)
    except (TypeError, ValueError):
        return Width.F
    if math.isnan(s) or s < 0.0:
        return Width.F
    if s < c.c_quarter:        return Width.Q
    if s < c.c_half:           return Width.H
    if s < c.c_three_quarter:  return Width.TQ
    return Width.F


def _is_finite_for_argmax(x: float, allow_pos_inf: bool = True) -> bool:
    if math.isnan(x):
        return False
    if math.isinf(x) and x < 0:
        return False
    if math.isinf(x) and not allow_pos_inf:
        return False
    return True


def route(sigma: float, logits: Sequence[float],
          cutoffs: Optional[Cutoffs] = None) -> Activation:
    c = cutoffs if cutoffs is not None else DEFAULT_CUTOFFS
    if not logits:
        return Activation(expert_id=-1, width=Width.F, score=float("nan"))
    best_i = -1
    best_v = -1e30
    for i, v in enumerate(logits):
        if not _is_finite_for_argmax(v):
            continue
        if v > best_v:
            best_v, best_i = v, i
    if best_i < 0:
        return Activation(expert_id=0, width=_pick_width(sigma, c),
                          score=float("nan"))
    return Activation(expert_id=best_i,
                      width=_pick_width(sigma, c),
                      score=best_v)


def top_k_route(sigma: float, logits: Sequence[float], k: int,
                cutoffs: Optional[Cutoffs] = None) -> List[Activation]:
    if not logits or k <= 0:
        return []
    c = cutoffs if cutoffs is not None else DEFAULT_CUTOFFS
    w = _pick_width(sigma, c)
    k = min(k, len(logits))
    visited = [False] * len(logits)
    out: List[Activation] = []
    for _ in range(k):
        best_i = -1
        best_v = -1e30
        for i, v in enumerate(logits):
            if visited[i] or not _is_finite_for_argmax(v):
                continue
            if v > best_v:
                best_v, best_i = v, i
        if best_i < 0:
            break
        visited[best_i] = True
        out.append(Activation(expert_id=best_i, width=w, score=best_v))
    return out


def compute_saved(acts: Sequence[Activation]) -> float:
    if not acts:
        return 0.0
    return sum(1.0 - a.width_frac for a in acts) / len(acts)
