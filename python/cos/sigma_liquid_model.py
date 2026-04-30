# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
**Liquid-style** adaptive forward scaffold + σ interrupt: checkpoint before a mutating
forward, probe activations, rollback on ``RETHINK``, fail closed on ``ABSTAIN``.

This is a **research adapter** around any object exposing ``__call__``, ``save_state``,
``load_state``, and ``adjust_tau`` — not a weight-compatible LFM2.5 checkpoint loader.
"""
from __future__ import annotations

from typing import Any, Callable, Optional, Protocol, Tuple, TypeVar

from .sigma_gate_core import SigmaState, Verdict, sigma_gate, sigma_update

T = TypeVar("T")


class _LiquidLike(Protocol[T]):
    def __call__(self, x: T) -> T: ...

    def save_state(self) -> Any: ...

    def load_state(self, checkpoint: Any) -> None: ...

    def adjust_tau(self, *, factor: float) -> None: ...


class SigmaLiquidModel:
    """
    Wrap a liquid-style ``model``; on ``RETHINK`` restore checkpoint, shrink ``tau``, and
    re-run once; on ``ABSTAIN`` return ``None``.
    """

    def __init__(
        self,
        model: _LiquidLike[Any],
        *,
        probe: Callable[[Any], float],
        k_raw: float = 0.9,
    ) -> None:
        self.model = model
        self.probe = probe
        self.k_raw = float(k_raw)

    def forward(self, x: Any, state: SigmaState) -> Tuple[Optional[Any], Verdict]:
        checkpoint = self.model.save_state()
        output = self.model(x)
        sp = max(0.0, min(1.0, float(self.probe(output))))
        kr = max(0.0, min(1.0, float(self.k_raw)))
        sigma_update(state, sp, kr)
        verdict = sigma_gate(state)

        if verdict == Verdict.ACCEPT:
            return output, verdict

        if verdict == Verdict.RETHINK:
            self.model.load_state(checkpoint)
            self.model.adjust_tau(factor=0.5)
            output = self.model(x)
            sp2 = max(0.0, min(1.0, float(self.probe(output))))
            sigma_update(state, sp2, kr)
            verdict = sigma_gate(state)
            if verdict == Verdict.ABSTAIN:
                return None, verdict
            return output, verdict

        return None, verdict


__all__ = ["SigmaLiquidModel"]
