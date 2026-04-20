# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""Python mirror of ``src/sigma/pipeline/ttt.h``.

Tracks a fast/slow weight pair, applies σ-gated updates, computes drift
(``||fast − slow|| / ||slow||``), and resets fast → slow when drift
exceeds the cap.  Pure Python — no numpy — so this stays importable on
any host without extra deps.

The parity test in ``benchmarks/sigma_pipeline/check_sigma_parity.py``
replays a canonical 4-step sequence through both implementations and
asserts agreement to 1e-6.
"""

from __future__ import annotations

import dataclasses
import math
from typing import List, Sequence


_NORM_EPS = 1e-12


@dataclasses.dataclass
class TTTState:
    fast: List[float]
    slow: List[float]
    lr: float
    tau_sigma: float
    tau_drift: float
    n_steps_total: int = 0
    n_steps_updated: int = 0
    n_resets: int = 0

    @classmethod
    def init(cls, slow: Sequence[float], lr: float,
             tau_sigma: float, tau_drift: float) -> "TTTState":
        if not slow or lr <= 0 or not (0.0 <= tau_sigma <= 1.0) \
                or tau_drift <= 0:
            raise ValueError("bad init args")
        return cls(
            fast=list(slow), slow=list(slow),
            lr=float(lr), tau_sigma=float(tau_sigma),
            tau_drift=float(tau_drift),
        )


def step(st: TTTState, sigma: float, grad: Sequence[float]) -> int:
    """σ-gated step.  Returns 1 if updated, 0 if skipped."""
    st.n_steps_total += 1
    try:
        s = float(sigma)
    except (TypeError, ValueError):
        return 0
    if math.isnan(s) or s < st.tau_sigma:
        return 0
    if len(grad) != len(st.fast):
        raise ValueError("grad length mismatch")
    for i, g in enumerate(grad):
        try:
            gv = float(g)
        except (TypeError, ValueError):
            continue
        if math.isnan(gv) or math.isinf(gv):
            continue
        st.fast[i] -= st.lr * gv
    st.n_steps_updated += 1
    return 1


def drift(st: TTTState) -> float:
    num2 = 0.0
    den2 = 0.0
    for f, s in zip(st.fast, st.slow):
        d = f - s
        num2 += d * d
        den2 += s * s
    num = math.sqrt(num2)
    den = math.sqrt(den2)
    if den < _NORM_EPS:
        return 0.0 if num < _NORM_EPS else 1.0
    return num / den


def reset_if_drift(st: TTTState) -> int:
    d = drift(st)
    if not (d > st.tau_drift):
        return 0
    st.fast = list(st.slow)
    st.n_resets += 1
    return 1
