# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""SIMULATE: digital twin / outcome probe before ACT (inject ``world``)."""
from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Tuple

from cos.sigma_gate_core import Verdict

from .state import OmegaContext


@dataclass
class OmegaSimulate:
    world: Any = None

    def __call__(self, ctx: OmegaContext, action: Any) -> Tuple[Verdict, Any, float]:
        if self.world is not None and hasattr(self.world, "simulate"):
            outcome = self.world.simulate(action)
            neg = bool(getattr(outcome, "negative", False))
            sig = float(getattr(outcome, "sigma", 0.42))
        else:
            outcome = {"simulated": True}
            neg, sig = False, 0.32
        return (Verdict.RETHINK if neg else Verdict.ACCEPT), outcome, sig
