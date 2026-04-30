# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""PREDICT: JEPA / world-model prior (inject ``jepa`` for research)."""
from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Dict, Tuple

from .state import OmegaContext


@dataclass
class OmegaPredict:
    jepa: Any = None

    def __call__(self, ctx: OmegaContext, perception: Dict[str, Any]) -> Tuple[Dict[str, Any], float]:
        if self.jepa is not None and hasattr(self.jepa, "predict"):
            pred = self.jepa.predict(ctx.scratch.get("world_state"), perception)
            sig = float(getattr(self.jepa, "last_sigma", 0.35))
        else:
            pred = {"prediction": "stub", "goal": ctx.goal[:128]}
            sig = 0.35
        ctx.scratch["prediction"] = pred
        return pred, sig
