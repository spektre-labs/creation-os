# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""WATCHDOG: halt on turn budget, σ streak, or optional energy hook."""
from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Callable, Optional

from cos.sigma_gate_core import SigmaState, Verdict


@dataclass
class OmegaWatchdog:
    max_turns: int = 50_000
    sigma_ceiling: float = 0.96
    energy_budget_exceeded: Optional[Callable[[], bool]] = None

    def __call__(self, master: SigmaState, turn: int) -> Verdict:
        if turn > self.max_turns:
            return Verdict.ABSTAIN
        if float(master.sigma) / 65536.0 > self.sigma_ceiling:
            return Verdict.ABSTAIN
        if self.energy_budget_exceeded is not None and self.energy_budget_exceeded():
            return Verdict.ABSTAIN
        return Verdict.ACCEPT
