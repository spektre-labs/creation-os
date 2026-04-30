# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
**σ sim-to-real:** track ``|σ_real − σ_sim|`` against a threshold and optionally trigger a
hooked **TTT / domain-adapt** step when the gap is large.

This is a **runtime harness** fragment — not a substitute for full domain-randomized
training claims without measured JSON. See ``docs/CLAIM_DISCIPLINE.md``.
"""
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any, Callable, Iterable, Optional, Tuple

from .sigma_gate_core import Verdict


@dataclass
class SigmaSim2Real:
    """Gap detector + optional ``ttt_update`` callback on ``RETHINK``."""

    threshold: float = 0.25
    compute_sigma: Callable[[Any], float] = field(
        default_factory=lambda: (lambda _out: 0.5)  # type: ignore[misc]
    )
    get_sim_baseline: Callable[[Any], float] = field(
        default_factory=lambda: (lambda _obs: 0.35)  # type: ignore[misc]
    )
    ttt_update: Optional[Callable[[Any, Any, Any], None]] = None

    def __post_init__(self) -> None:
        self.threshold = max(1e-9, float(self.threshold))

    def track_gap(self, sim_sigma: float, real_sigma: float) -> Tuple[Verdict, float]:
        """Return ``(verdict, gap)`` where a large gap maps to ``RETHINK`` (revisit policy)."""
        gap = abs(float(real_sigma) - float(sim_sigma))
        if gap > self.threshold:
            return Verdict.RETHINK, gap
        return Verdict.ACCEPT, gap

    def adapt(self, model: Any, real_observations: Iterable[Any]) -> int:
        """
        For each observation, compare real output σ vs sim baseline; on ``RETHINK`` call
        ``ttt_update(model, obs, output)`` if configured. Returns number of TTT steps run.
        """
        n = 0
        ttt = self.ttt_update
        for obs in real_observations:
            output = model(obs)
            real_sigma = max(0.0, min(1.0, float(self.compute_sigma(output))))
            sim_sigma = max(0.0, min(1.0, float(self.get_sim_baseline(obs))))
            verdict, _gap = self.track_gap(sim_sigma, real_sigma)
            if verdict == Verdict.RETHINK and ttt is not None:
                ttt(model, obs, output)
                n += 1
        return n


__all__ = ["SigmaSim2Real"]
