# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""Python mirror of ``src/sigma/pipeline/continual.{h,c}``."""

from __future__ import annotations

import dataclasses
import math
from typing import List, Sequence


def _safe_sq(x: float) -> float:
    if math.isnan(x) or math.isinf(x):
        return 0.0
    return x * x


@dataclasses.dataclass
class Continual:
    n: int
    freeze_threshold: float
    decay: float = 0.99

    def __post_init__(self) -> None:
        if self.n <= 0:
            raise ValueError("n must be positive")
        if not (self.freeze_threshold > 0):
            raise ValueError("freeze_threshold must be > 0")
        if not (0.0 <= self.decay <= 1.0):
            raise ValueError("decay must be in [0, 1]")
        self.importance: List[float] = [0.0] * self.n
        self.n_steps = 0
        self.frozen_count = 0
        self.n_masked_total = 0

    def observe_gradient(self, grad: Sequence[float]) -> int:
        newly_frozen = 0
        frozen_now = 0
        thr = self.freeze_threshold
        for i in range(self.n):
            was = self.importance[i] > thr
            self.importance[i] = (self.decay * self.importance[i]
                                  + (1.0 - self.decay) * _safe_sq(grad[i]))
            is_ = self.importance[i] > thr
            if is_: frozen_now += 1
            if not was and is_: newly_frozen += 1
        self.frozen_count = frozen_now
        self.n_steps += 1
        return newly_frozen

    def apply_mask(self, grad: List[float]) -> int:
        zeros = 0
        thr = self.freeze_threshold
        for i in range(self.n):
            if self.importance[i] > thr:
                grad[i] = 0.0
                zeros += 1
        self.n_masked_total += zeros
        return zeros

    def step(self, grad: List[float]) -> int:
        self.observe_gradient(grad)
        return self.apply_mask(grad)

    def decay_tick(self) -> None:
        frozen_now = 0
        thr = self.freeze_threshold
        for i in range(self.n):
            self.importance[i] *= self.decay
            if self.importance[i] > thr:
                frozen_now += 1
        self.frozen_count = frozen_now
