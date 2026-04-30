# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
Shared Ω-loop context and phase index (14 lanes, aligned with ``omega_phase_gates.h``).
"""
from __future__ import annotations

from dataclasses import dataclass, field
from enum import IntEnum
from typing import Any, Dict, List

from cos.sigma_gate_core import SigmaState, Verdict, sigma_gate, sigma_update


class OmegaPhase(IntEnum):
    PERCEIVE = 0
    PREDICT = 1
    REMEMBER = 2
    THINK = 3
    GATE = 4
    SAFETY = 5
    SIMULATE = 6
    ACT = 7
    PROVE = 8
    LEARN = 9
    REFLECT = 10
    CONSOLIDATE = 11
    WATCHDOG = 12
    CONTINUE = 13


PHASE_NAMES: tuple[str, ...] = tuple(p.name for p in OmegaPhase)
N_PHASES: int = len(PHASE_NAMES)


@dataclass
class OmegaContext:
    """Mutable scratch passed through one Ω turn."""

    goal: str
    turn: int
    scratch: Dict[str, Any] = field(default_factory=dict)


@dataclass
class OmegaState:
    """Per-run ledger: one ``SigmaState`` per phase + master (harness mirror)."""

    master: SigmaState = field(default_factory=SigmaState)
    phases: List[SigmaState] = field(default_factory=lambda: [SigmaState() for _ in range(N_PHASES)])
    turn: int = 0
    history: List[Dict[str, Any]] = field(default_factory=list)


def phase_verdict(sigma_01: float, k_raw: float) -> Verdict:
    """Fresh-lane interrupt for one scalar σ (matches per-phase C bundle semantics)."""
    st = SigmaState()
    s = max(0.0, min(1.0, float(sigma_01)))
    kr = max(0.0, min(1.0, float(k_raw)))
    sigma_update(st, s, kr)
    return sigma_gate(st)


__all__ = [
    "N_PHASES",
    "OmegaContext",
    "OmegaPhase",
    "OmegaState",
    "PHASE_NAMES",
    "phase_verdict",
]
