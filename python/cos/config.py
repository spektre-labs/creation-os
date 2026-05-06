# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
# Source:        https://github.com/spektre-labs/creation-os-kernel
# Website:       https://spektrelabs.org
"""Unified σ-gate configuration. Single source of truth for threshold naming.

Use :data:`DEFAULT_CONFIG` for package defaults and :class:`SigmaConfig` for
immutable bands and verdict helpers (independent of :class:`~cos.sigma_gate.SigmaGate`
boundary semantics).
"""
from __future__ import annotations

from dataclasses import dataclass, replace


@dataclass(frozen=True, slots=True)
class SigmaConfig:
    """Immutable σ-gate threshold band (accept / rethink / abstain).

    Usage::

        config = SigmaConfig()
        config = SigmaConfig(threshold_accept=0.1)
        new_cfg = config.with_thresholds(accept=0.2)

    Verdict mapping uses strict abstain cut: ``σ > threshold_abstain`` → ABSTAIN.
    """

    threshold_accept: float = 0.15
    threshold_abstain: float = 0.85

    def __post_init__(self) -> None:
        if not (0.0 <= self.threshold_accept < self.threshold_abstain <= 1.0):
            raise ValueError(
                f"Invalid thresholds: accept={self.threshold_accept}, "
                f"abstain={self.threshold_abstain}. "
                "Must satisfy 0 ≤ accept < abstain ≤ 1."
            )

    def verdict(self, sigma: float) -> str:
        """Map scalar σ to a string verdict (ACCEPT / RETHINK / ABSTAIN)."""
        s = float(sigma)
        if s < self.threshold_accept:
            return "ACCEPT"
        if s > self.threshold_abstain:
            return "ABSTAIN"
        return "RETHINK"

    def with_thresholds(
        self,
        *,
        accept: float | None = None,
        abstain: float | None = None,
    ) -> SigmaConfig:
        """Return a new config with updated thresholds."""
        return replace(
            self,
            threshold_accept=float(self.threshold_accept if accept is None else accept),
            threshold_abstain=float(self.threshold_abstain if abstain is None else abstain),
        )

    @classmethod
    def from_persona(cls, persona: str) -> SigmaConfig:
        """Preset configurations per deployment context (UX / policy hints)."""
        presets: dict[str, SigmaConfig] = {
            "default": cls(threshold_accept=0.15, threshold_abstain=0.85),
            "automotive": cls(threshold_accept=0.10, threshold_abstain=0.50),
            "medical": cls(threshold_accept=0.08, threshold_abstain=0.40),
            "creative": cls(threshold_accept=0.50, threshold_abstain=0.95),
            "research": cls(threshold_accept=0.30, threshold_abstain=0.80),
        }
        if persona not in presets:
            raise ValueError(f"Unknown persona: {persona!r}. Options: {sorted(presets)}")
        return presets[persona]


DEFAULT_CONFIG = SigmaConfig()


__all__ = ["DEFAULT_CONFIG", "SigmaConfig"]
