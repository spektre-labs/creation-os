# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""Python mirror of ``src/sigma/pipeline/swarm.{h,c}``."""

from __future__ import annotations

import dataclasses
import enum
import math
from typing import List, Optional, Sequence


class Verdict(enum.Enum):
    ABSTAIN   = 0
    CONSENSUS = 1
    ACCEPT    = 2


@dataclasses.dataclass
class SwarmResponse:
    sigma: float
    cost_eur: float = 0.0
    model_id: Optional[str] = None
    text: Optional[str] = None   # Python-only convenience


@dataclasses.dataclass
class SwarmDecision:
    verdict: Verdict
    winner_idx: int
    winner_sigma: float
    sigma_swarm: float
    mean_weight: float
    total_cost_eur: float
    winner_text: Optional[str] = None
    winner_model: Optional[str] = None


def _clamp01(x: float) -> float:
    if math.isnan(x): return 1.0
    if x < 0.0: return 0.0
    if x > 1.0: return 1.0
    return x


def consensus(resps: Sequence[SwarmResponse],
              tau_abstain: float = 0.30,
              tau_accept: float = 0.60) -> SwarmDecision:
    if not resps:
        return SwarmDecision(Verdict.ABSTAIN, -1, 1.0, 1.0, 0.0, 0.0)
    sum_w = 0.0
    total_cost = 0.0
    best_i = -1
    best_w = -1.0
    best_s = 2.0
    for i, r in enumerate(resps):
        s = _clamp01(r.sigma)
        w = max(0.0, 1.0 - s)
        sum_w += w
        total_cost += r.cost_eur
        if w > best_w or (w == best_w and s < best_s):
            best_w, best_s, best_i = w, s, i

    n = len(resps)
    mean_w = sum_w / n
    sigma_swarm = _clamp01(1.0 - mean_w)

    if mean_w <= 0.0:
        return SwarmDecision(Verdict.ABSTAIN, -1, 1.0, sigma_swarm,
                             mean_w, total_cost)

    if mean_w < tau_abstain:   v = Verdict.ABSTAIN
    elif mean_w >= tau_accept: v = Verdict.ACCEPT
    else:                      v = Verdict.CONSENSUS

    winner = resps[best_i]
    return SwarmDecision(
        verdict=v,
        winner_idx=best_i,
        winner_sigma=best_s,
        sigma_swarm=sigma_swarm,
        mean_weight=mean_w,
        total_cost_eur=total_cost,
        winner_text=winner.text,
        winner_model=winner.model_id,
    )


def should_escalate(resps: Sequence[SwarmResponse],
                    n_used: int, n_total: int,
                    tau_stop: float = 0.30) -> bool:
    if not resps:     return False
    if n_used >= n_total: return False
    if n_used <= 0:   return True
    best = 1.0
    for i in range(n_used):
        s = _clamp01(resps[i].sigma)
        if s < best:
            best = s
    return best > tau_stop


def cost_aware_consensus(resps: Sequence[SwarmResponse],
                         tau_stop: float = 0.30,
                         tau_abstain: float = 0.30,
                         tau_accept: float = 0.60
                         ) -> SwarmDecision:
    """Walk the escalation ladder cheapest→dearest until best σ ≤ tau_stop.

    The input ``resps`` is assumed already sorted cheap-first.  Only
    the responses we actually "used" are passed to ``consensus``.
    """
    n_total = len(resps)
    n_used = min(1, n_total)
    while should_escalate(resps, n_used, n_total, tau_stop):
        n_used += 1
    return consensus(resps[:n_used], tau_abstain, tau_accept)
