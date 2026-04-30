# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""CONTINUE: ``should_continue`` wrapper (∆σ / interrupt hygiene)."""
from __future__ import annotations

from dataclasses import dataclass

from cos.sigma_gate_core import SigmaState, should_continue


@dataclass
class OmegaContinue:
    def __call__(self, master: SigmaState) -> bool:
        return should_continue(master)
