# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-quantize — per-layer bit-width map from σ (lab mixed-precision policy).

Low σ suggests calmer activations in this proxy; high σ keeps more bits. Not a calibrated
PTQ run; see ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

from typing import Dict

__all__ = ["SigmaQuantize"]


class SigmaQuantize:
    """Map layer σ to integer bit widths (2 / 4 / 8 / 16)."""

    @staticmethod
    def layer_bits(sigma: float) -> int:
        s = max(0.0, min(1.0, float(sigma)))
        if s < 0.25:
            return 2
        if s < 0.5:
            return 4
        if s < 0.75:
            return 8
        return 16

    def mixed_precision_map(self, layer_sigmas: Dict[str, float]) -> Dict[str, int]:
        return {str(k): self.layer_bits(v) for k, v in layer_sigmas.items()}
