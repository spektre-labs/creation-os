# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""THINK: σ-governed reasoning (:class:`cos.sigma_reasoning.SigmaReasoning`) or stub."""
from __future__ import annotations

from dataclasses import dataclass
from typing import Any, List, Tuple

from .state import OmegaContext


@dataclass
class OmegaThink:
    reasoner: Any = None

    def __call__(self, ctx: OmegaContext, memory: Any, prediction: Any) -> Tuple[List[str], float]:
        if self.reasoner is not None:
            out = self.reasoner.reason(ctx.goal, max_depth=6)
            sig = 0.22 if out else 0.55
            thoughts = [str(out)] if out else []
            return thoughts, sig
        thoughts = [f"think:{ctx.goal[:64]}"]
        return thoughts, 0.28
