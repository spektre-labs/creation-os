# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""Python mirror of ``src/sigma/pipeline/tinyml.{h,c}``.

Welford-on-σ streaming anomaly detector.  Semantic parity with C is
strong but not bit-identical: Python floats are 64-bit while the C
primitive is 32-bit, so tolerance is ~1e-4 on the demo sweep.
"""

from __future__ import annotations

import dataclasses
import math
from typing import List

Z_ALERT = 3.0
EPS     = 1e-6


@dataclasses.dataclass
class TinyState:
    mean: float = 0.0
    m2:   float = 0.0
    count: int = 0
    last_sigma_q: int = 0

    @property
    def state_bytes(self) -> int:
        # Matches the 12-byte C layout: 4 + 4 + 2 + 2.
        return 12


def _clamp01(x: float) -> float:
    if math.isnan(x): return 1.0
    if x < 0.0: return 0.0
    if x > 1.0: return 1.0
    return x


def _quantise(sigma: float) -> int:
    s = _clamp01(sigma) * 65535.0 + 0.5
    if s < 0: s = 0
    if s > 65535: s = 65535
    return int(s)


def observe(state: TinyState, value: float) -> float:
    if math.isnan(value) or math.isinf(value):
        state.last_sigma_q = _quantise(1.0)
        return 1.0

    sigma = 0.0
    if state.count >= 2:
        var = state.m2 / (state.count - 1)
        std = math.sqrt(var)
        z   = abs(value - state.mean) / (std + EPS)
        sigma = _clamp01(z / Z_ALERT)

    n = state.count + 1
    if n > 65535: n = 65535
    delta = value - state.mean
    state.mean += delta / n
    delta2 = value - state.mean
    state.m2 += delta * delta2
    state.count = n
    state.last_sigma_q = _quantise(sigma)
    return sigma


def last_sigma(state: TinyState) -> float:
    return state.last_sigma_q / 65535.0


def is_anomalous(state: TinyState, threshold: float) -> bool:
    return last_sigma(state) > threshold


def replay(values: List[float]) -> List[float]:
    st = TinyState()
    return [observe(st, v) for v in values]
