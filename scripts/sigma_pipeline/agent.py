# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""Python mirror of ``src/sigma/pipeline/agent.{h,c}``."""

from __future__ import annotations

import dataclasses
import enum
import math


class Phase(enum.IntEnum):
    OBSERVE = 0
    ORIENT  = 1
    DECIDE  = 2
    ACT     = 3
    REFLECT = 4


class Decision(enum.IntEnum):
    BLOCK   = 0
    CONFIRM = 1
    ALLOW   = 2


class ActionClass(enum.IntEnum):
    READ          = 0
    WRITE         = 1
    NETWORK       = 2
    IRREVERSIBLE  = 3


_MIN_AUTONOMY = {
    ActionClass.READ:         0.30,
    ActionClass.WRITE:        0.60,
    ActionClass.NETWORK:      0.80,
    ActionClass.IRREVERSIBLE: 0.95,
}


def min_autonomy(cls: ActionClass) -> float:
    return _MIN_AUTONOMY.get(cls, 1.01)


def _clamp01(x: float) -> float:
    if math.isnan(x):
        return 1.0
    return max(0.0, min(1.0, x))


@dataclasses.dataclass
class Agent:
    base_autonomy: float
    confirm_margin: float

    def __post_init__(self) -> None:
        if not (0.0 <= self.base_autonomy <= 1.0):
            raise ValueError("base_autonomy must be in [0, 1]")
        if not (0.0 < self.confirm_margin < 1.0):
            raise ValueError("confirm_margin must be in (0, 1)")
        self.phase = Phase.OBSERVE
        self.n_cycles = 0
        self.last_sigma = 0.0
        self.sum_sigma_pred = 0.0
        self.sum_sigma_obs  = 0.0
        self.sum_abs_err    = 0.0

    def advance(self, expected: Phase) -> None:
        if self.phase != expected:
            raise RuntimeError(f"out of sequence: at {self.phase.name}, expected {expected.name}")
        if self.phase == Phase.REFLECT:
            self.phase = Phase.OBSERVE
            self.n_cycles += 1
        else:
            self.phase = Phase(int(self.phase) + 1)

    def gate(self, cls: ActionClass, sigma: float) -> Decision:
        if math.isnan(sigma):
            return Decision.BLOCK
        s = _clamp01(sigma)
        eff = self.base_autonomy * (1.0 - s)
        req = min_autonomy(cls)
        if req > 1.0:
            return Decision.BLOCK
        if eff >= req:
            return Decision.ALLOW
        if eff >= req - self.confirm_margin:
            return Decision.CONFIRM
        return Decision.BLOCK

    def reflect(self, sigma_pred: float, sigma_obs: float) -> None:
        p = _clamp01(sigma_pred)
        o = _clamp01(sigma_obs)
        self.sum_sigma_pred += p
        self.sum_sigma_obs  += o
        self.sum_abs_err    += abs(p - o)
        self.last_sigma      = o

    def calibration_err(self) -> float:
        if self.n_cycles <= 0:
            return 0.0
        return self.sum_abs_err / self.n_cycles
