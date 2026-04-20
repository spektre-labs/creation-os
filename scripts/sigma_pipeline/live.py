# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""Python mirror of ``src/sigma/pipeline/live.{h,c}``.

Semantic parity with the C primitive; σ quantisation also uses
uint16 so the Python mirror picks the same discrete τ boundaries
as the C binary (modulo single-ULP quantisation jitter).
"""

from __future__ import annotations

import dataclasses
from typing import List


def _clamp(x: float, lo: float, hi: float) -> float:
    if x != x:
        return lo
    if x < lo: return lo
    if x > hi: return hi
    return x


def _quant(sigma: float) -> int:
    s = _clamp(sigma, 0.0, 1.0) * 65535.0 + 0.5
    return int(s)


def _dequant(q: int) -> float:
    return q / 65535.0


@dataclasses.dataclass
class Observation:
    sigma_q: int
    correct: bool


@dataclasses.dataclass
class Live:
    capacity: int = 64
    seed_tau_accept: float = 0.30
    seed_tau_rethink: float = 0.60
    tau_min: float = 0.0
    tau_max: float = 1.0
    target_accept: float = 0.95
    target_rethink: float = 0.50
    min_samples: int = 16

    def __post_init__(self) -> None:
        self._buf: List[Observation] = []
        self.head = 0
        self.total_seen = 0
        self.correct_seen = 0
        self.tau_accept  = _clamp(self.seed_tau_accept,  self.tau_min, self.tau_max)
        self.tau_rethink = _clamp(self.seed_tau_rethink, self.tau_min, self.tau_max)
        if self.tau_rethink < self.tau_accept:
            self.tau_rethink = self.tau_accept
            self.seed_tau_rethink = self.tau_accept
        self.accept_accuracy = 0.0
        self.rethink_accuracy = 0.0
        self.n_adapts = 0

    @property
    def count(self) -> int:
        return len(self._buf)

    def observe(self, sigma: float, correct: bool) -> None:
        o = Observation(sigma_q=_quant(sigma), correct=bool(correct))
        if self.count < self.capacity:
            self._buf.append(o)
        else:
            self._buf[self.head] = o
            self.head = (self.head + 1) % self.capacity
        self.total_seen += 1
        if correct:
            self.correct_seen += 1
        self.adapt()

    def adapt(self) -> None:
        self.n_adapts += 1
        if self.count < self.min_samples:
            self.tau_accept       = self.seed_tau_accept
            self.tau_rethink      = self.seed_tau_rethink
            self.accept_accuracy  = 0.0
            self.rethink_accuracy = 0.0
            return
        sorted_obs = sorted(self._buf, key=lambda o: o.sigma_q)
        correct = 0
        tau_a = self.seed_tau_accept
        tau_r = self.seed_tau_rethink
        acc_a = 0.0
        acc_r = 0.0
        for i, o in enumerate(sorted_obs):
            if o.correct:
                correct += 1
            acc = correct / (i + 1)
            sig = _dequant(o.sigma_q)
            if acc >= self.target_accept:
                tau_a = sig
                acc_a = acc
            if acc >= self.target_rethink:
                tau_r = sig
                acc_r = acc
        tau_a = _clamp(tau_a, self.tau_min, self.tau_max)
        tau_r = _clamp(tau_r, self.tau_min, self.tau_max)
        if tau_r < tau_a:
            tau_r = tau_a
        self.tau_accept       = tau_a
        self.tau_rethink      = tau_r
        self.accept_accuracy  = acc_a
        self.rethink_accuracy = acc_r

    @property
    def lifetime_accuracy(self) -> float:
        if self.total_seen == 0:
            return 0.0
        return self.correct_seen / self.total_seen
