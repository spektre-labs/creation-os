# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""REFLECT: lightweight meta-cognition over per-turn σ records."""
from __future__ import annotations

from dataclasses import dataclass
from statistics import mean
from typing import Any, Dict, List, Tuple


@dataclass
class OmegaReflect:
    def __call__(self, turn_history: List[Dict[str, Any]]) -> Tuple[str, float]:
        if not turn_history:
            return "STABLE", 0.5
        sigs = [float(t.get("max_sigma", 0.5)) for t in turn_history]
        avg = float(mean(sigs)) if sigs else 0.5
        trend = float(sigs[-1] - sigs[0]) if len(sigs) > 1 else 0.0
        if trend > 1e-6:
            return "DRIFTING", avg
        if avg < 0.2:
            return "STABLE", avg
        return "AT_RISK", avg
