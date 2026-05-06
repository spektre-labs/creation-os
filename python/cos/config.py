# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
# Source:        https://github.com/spektre-labs/creation-os-kernel
# Website:       https://spektrelabs.org
"""Canonical σ-gate band configuration (threshold naming).

Use :data:`DEFAULT_CONFIG` for package-wide defaults and :class:`SigmaConfig` for
pure verdict logic without constructing a :class:`~cos.sigma_gate.SigmaGate`.
"""
from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True)
class SigmaConfig:
    """Band thresholds for mapping scalar σ to ACCEPT / RETHINK / ABSTAIN."""

    threshold_accept: float = 0.15
    threshold_abstain: float = 0.85

    def verdict(self, sigma: float) -> str:
        s = float(sigma)
        if s < self.threshold_accept:
            return "ACCEPT"
        if s < self.threshold_abstain:
            return "RETHINK"
        return "ABSTAIN"


DEFAULT_CONFIG = SigmaConfig()


__all__ = ["DEFAULT_CONFIG", "SigmaConfig"]
