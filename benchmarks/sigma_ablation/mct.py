# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""Monte Carlo Temperature (MCT) — per-sample random temperature in [low, high]."""

from __future__ import annotations

import random
from typing import List


def mct_temperatures(n_samples: int, low: float, high: float, seed: int) -> List[float]:
    """Return ``n_samples`` i.i.d. temperatures uniform in ``[low, high]``."""
    rng = random.Random(int(seed) & 0xFFFFFFFF)
    lo, hi = float(low), float(high)
    if hi < lo:
        lo, hi = hi, lo
    return [float(rng.uniform(lo, hi)) for _ in range(int(n_samples))]


def fixed_temperatures(n_samples: int, temp: float) -> List[float]:
    """All samples at the same temperature."""
    t = float(temp)
    return [t] * int(n_samples)
