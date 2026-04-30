# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""GATE: multi-signal cascade (:class:`cos.sigma_fusion.SigmaFusion`)."""
from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Dict, Tuple

from cos.sigma_fusion import SigmaFusion

from .state import OmegaContext


@dataclass
class OmegaGate:
    fusion: Any = None

    def __post_init__(self) -> None:
        if self.fusion is None:
            self.fusion = SigmaFusion()

    def __call__(self, ctx: OmegaContext, scores: Dict[str, Any]) -> Tuple[Dict[str, Any], float]:
        clean = {k: float(v) for k, v in scores.items() if v is not None}
        v, pooled, note = self.fusion.cascade(clean)
        out = {"verdict": int(v), "pooled": float(pooled), "note": note}
        return out, float(pooled)
