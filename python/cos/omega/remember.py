# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""REMEMBER: σ-engram recall (:class:`cos.sigma_engram.SigmaEngram`)."""
from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Optional, Tuple

from cos.sigma_engram import SigmaEngram

from .state import OmegaContext


@dataclass
class OmegaRemember:
    engram: Any = None

    def __post_init__(self) -> None:
        if self.engram is None:
            self.engram = SigmaEngram(max_entries=4096)

    def __call__(self, ctx: OmegaContext, query: str) -> Tuple[Optional[Tuple[str, str, float]], float]:
        hits = self.engram.recall(query, tau=0.5)
        if not hits:
            return None, 0.5
        best = min(hits, key=lambda x: float(x[2]))
        return best, float(best[2])
