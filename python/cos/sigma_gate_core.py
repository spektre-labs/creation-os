# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
Python mirror of ``sigma_gate.h``: Q16 cognitive interrupt state + verdict.

**Spec source:** ``tests/test_sigma_gate_core.py`` + C reference in ``tests/test_sigma_gate.c``.
"""
from __future__ import annotations

from dataclasses import dataclass
from enum import IntEnum
from typing import Any

# Fixed-point scale: σ and k live in [0, Q16] representing [0, 1).
Q16 = 1 << 16
SIGMA_HALF = Q16 // 2
# Critical k_eff below which the gate abstains (matches tests / header ratio 0.127).
K_CRIT = int(0.127 * Q16)


class Verdict(IntEnum):
    ACCEPT = 0
    RETHINK = 1
    ABSTAIN = 2


@dataclass
class SigmaState:
    """Q16 cognitive state (``sigma``, ``d_sigma``, ``k_eff``)."""

    sigma: int = 0
    d_sigma: int = 0
    k_eff: int = Q16


def sigma_q16(x: Any) -> int:
    """Map a scalar in ~[0, 1] to Q16; clamps to [0, Q16]."""
    try:
        v = float(x)
    except (TypeError, ValueError):
        v = 0.0
    if v < 0.0:
        v = 0.0
    if v > 1.0:
        v = 1.0
    return int(round(v * float(Q16)))


def sigma_update_q16(st: SigmaState, new_sigma: int, k_raw: int) -> None:
    """In-place update: ``d_sigma`` is step delta; ``k_eff`` from residual × k_raw."""
    delta = int(new_sigma) - int(st.sigma)
    st.d_sigma = int(delta)
    st.sigma = int(new_sigma)
    ns = int(st.sigma)
    kr = int(k_raw)
    st.k_eff = ((Q16 - ns) * kr) >> 16


def sigma_update(st: SigmaState, sigma_01: float, k_raw_01: float) -> None:
    """Float API used by router / streaming: ``sigma_01``, ``k_raw_01`` in [0, 1]."""
    sigma_update_q16(st, sigma_q16(sigma_01), sigma_q16(k_raw_01))


def sigma_gate(st: SigmaState) -> Verdict:
    """Cognitive verdict from current Q16 state."""
    ke = int(st.k_eff)
    if ke < K_CRIT:
        return Verdict.ABSTAIN
    if int(st.d_sigma) > 0:
        return Verdict.RETHINK
    return Verdict.ACCEPT


def should_continue(st: SigmaState) -> bool:
    """True when the interrupt allows further decoding (not ABSTAIN)."""
    return sigma_gate(st) != Verdict.ABSTAIN


__all__ = [
    "K_CRIT",
    "Q16",
    "SIGMA_HALF",
    "SigmaState",
    "Verdict",
    "should_continue",
    "sigma_gate",
    "sigma_q16",
    "sigma_update",
    "sigma_update_q16",
]
