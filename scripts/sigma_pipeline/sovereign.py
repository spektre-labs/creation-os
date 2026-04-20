# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""Python mirror of ``src/sigma/pipeline/sovereign.{h,c}``."""

from __future__ import annotations

import dataclasses
import enum
import math


class Verdict(enum.IntEnum):
    DEPENDENT = 0
    HYBRID    = 1
    FULL      = 2


def _safe_eur(e: float) -> float:
    if math.isnan(e) or math.isinf(e):
        return 0.0
    return max(0.0, e)


@dataclasses.dataclass
class Sovereign:
    min_sovereign_fraction: float = 0.85

    def __post_init__(self) -> None:
        if not (0.0 <= self.min_sovereign_fraction <= 1.0):
            self.min_sovereign_fraction = 0.85
        self.n_local = 0
        self.n_cloud = 0
        self.n_abstain = 0
        self.eur_local_total = 0.0
        self.eur_cloud_total = 0.0
        self.bytes_sent_cloud = 0
        self.bytes_recv_cloud = 0

    def record_local(self, eur: float = 0.0) -> None:
        self.n_local += 1
        self.eur_local_total += _safe_eur(eur)

    def record_cloud(self, eur: float = 0.0,
                     sent_bytes: int = 0, recv_bytes: int = 0) -> None:
        self.n_cloud += 1
        self.eur_cloud_total += _safe_eur(eur)
        self.bytes_sent_cloud += max(0, sent_bytes)
        self.bytes_recv_cloud += max(0, recv_bytes)

    def record_abstain(self) -> None:
        self.n_abstain += 1

    def fraction(self) -> float:
        denom = self.n_local + self.n_cloud
        if denom == 0:
            return 1.0
        return self.n_local / denom

    def eur_per_call(self) -> float:
        total = self.n_local + self.n_cloud + self.n_abstain
        if total == 0:
            return 0.0
        return (self.eur_local_total + self.eur_cloud_total) / total

    def monthly_eur_estimate(self, calls_per_day: int) -> float:
        if calls_per_day < 0:
            calls_per_day = 0
        return self.eur_per_call() * calls_per_day * 30.0

    def verdict(self) -> Verdict:
        if self.n_cloud == 0:
            return Verdict.FULL
        if self.fraction() >= self.min_sovereign_fraction:
            return Verdict.HYBRID
        return Verdict.DEPENDENT
