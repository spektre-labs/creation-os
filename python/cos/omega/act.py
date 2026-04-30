# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""ACT: execute only when master coherence is above ``K_CRIT`` (harness mirror)."""
from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Optional, Tuple

from cos.sigma_gate_core import K_CRIT, SigmaState, Verdict

from .state import OmegaContext


@dataclass
class OmegaAct:
    execute: Any = None

    def __call__(self, ctx: OmegaContext, action: Any, master: SigmaState) -> Tuple[Optional[Any], float]:
        if int(master.k_eff) < int(K_CRIT):
            return None, 0.95
        if self.execute is not None:
            result = self.execute(action)
        else:
            result = {"executed": True, "action_repr": repr(action)[:256]}
        return result, 0.14
