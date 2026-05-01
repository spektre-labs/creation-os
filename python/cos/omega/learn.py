# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""LEARN: σ-gated TTT + engram (RETHINK learns; ACCEPT stores)."""
from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Dict, Optional

from cos.sigma_gate_core import Verdict

from .state import OmegaContext


@dataclass
class OmegaLearn:
    engram: Any = None
    ttt_update: Optional[Any] = None

    def __call__(self, ctx: OmegaContext, experience: Dict[str, Any], verdict: Verdict) -> None:
        if verdict == Verdict.RETHINK and self.ttt_update is not None:
            self.ttt_update(experience)
            return
        if verdict == Verdict.ACCEPT and self.engram is not None:
            p = str(experience.get("prompt", ctx.goal))
            r = str(experience.get("response", ""))
            if p and r and hasattr(self.engram, "store"):
                sig = float(experience.get("sigma", 0.2))
                self.engram.store(p, r, verdict, sig)
