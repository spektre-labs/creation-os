# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""PERCEIVE: multimodal σ via :class:`cos.sigma_sense.SigmaSense`."""
from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Dict, Tuple

from cos.sigma_sense import SigmaSense

from .state import OmegaContext


@dataclass
class OmegaPerceive:
    sense: Any = None

    def __post_init__(self) -> None:
        if self.sense is None:
            self.sense = SigmaSense()

    def __call__(self, ctx: OmegaContext, raw: Dict[str, Any]) -> Tuple[Dict[str, Any], float]:
        s, _modal = self.sense.perceive(raw)
        ctx.scratch["perceive_modal"] = _modal
        return raw, float(s)
