# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
**Living weights** scaffold: three-tier parameter hygiene (kernel / firmware / application).

**Kernel** references ``sigma_gate.h`` interrupt semantics — **never mutated** by this module.

**Firmware** vs **application** are NumPy buffers for slow / fast TTT-style updates in lab
harnesses. A PyTorch research path can wrap ``firmware`` / ``application`` in
``nn.Parameter`` at load time without changing interrupt semantics.

``σ`` routing (via ``Verdict``) selects which tier may receive an update step.
"""
from __future__ import annotations

from typing import Optional

import numpy as np

from .sigma_gate_core import Verdict


class SigmaLivingWeights:
    """Two trainable tiers (firmware slow / application fast); kernel is out-of-band."""

    def __init__(self, d_model: int) -> None:
        d = int(d_model)
        self.firmware = np.zeros((d, d), dtype=np.float64)
        self.application = np.zeros((d, d), dtype=np.float64)

    def target_array(self, verdict: Verdict) -> Optional[np.ndarray]:
        """Return the buffer that may be updated for ``verdict``, or ``None`` for ``ACCEPT``."""
        if verdict == Verdict.ACCEPT:
            return None
        if verdict == Verdict.RETHINK:
            return self.application
        return self.firmware

    def target_parameter(self, verdict: Verdict) -> Optional[np.ndarray]:
        """Alias of ``target_array`` (historical name; returns NumPy buffers, not ``torch`` params)."""
        return self.target_array(verdict)


__all__ = ["SigmaLivingWeights"]
