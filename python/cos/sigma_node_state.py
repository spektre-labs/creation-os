# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Node state machine labels for ``cos resolve --verify-trace`` (lab)."""
from __future__ import annotations

from cos.sigma_gate import SigmaGate  # noqa: F401 — σ kernel anchor (sigma_gate.h / Python lite)

from typing import List, Tuple


class NodeStateMachine:
    STATES: Tuple[str, ...] = ("IDLE", "ACTIVE", "VERIFY", "QUARANTINE", "RETIRED")

    def __init__(self, name: str = "lab") -> None:
        self.name = str(name)

    def is_valid_trace(self, trace: List[str]) -> Tuple[bool, str]:
        if not trace:
            return False, "empty trace"
        for step in trace:
            if step not in self.STATES:
                return False, f"unknown state {step!r}"
        return True, "ok"


__all__ = ["NodeStateMachine"]
