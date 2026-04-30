# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
σ-adaptive **inference budget** scaffold: map a cheap precheck scalar to token cap, reasoning
depth, and a symbolic model tier. Escalation strings are harness labels only (no vendor
claims).

See ``docs/CLAIM_DISCIPLINE.md`` — tier names are placeholders, not benchmark promises.
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Callable, Optional


@dataclass(frozen=True)
class Budget:
    """Discrete compute envelope chosen from precheck σ."""

    tokens: int
    reasoning_depth: int
    model: Optional[str]
    action: Optional[str] = None


@dataclass
class SigmaBudget:
    """
    Map ``precheck_sigma`` in ``[0, 1]`` (higher = riskier / needs more compute) to a
    :class:`Budget`. Override :meth:`precheck` for tokenizer- or router-backed scores.
    """

    precheck: Callable[[str], float]

    def allocate(self, prompt: str, state: Any = None) -> Budget:
        _ = state  # reserved for ledger / session hooks
        s = float(self.precheck(prompt))
        s = max(0.0, min(1.0, s))

        if s < 0.1:
            return Budget(tokens=10, reasoning_depth=0, model="local_tiny")
        if s < 0.3:
            return Budget(tokens=100, reasoning_depth=1, model="bitnet_2b")
        if s < 0.6:
            return Budget(tokens=500, reasoning_depth=3, model="qwen3_8b")
        if s < 0.85:
            return Budget(tokens=2000, reasoning_depth=8, model="gemma_4b")
        return Budget(tokens=0, reasoning_depth=0, model=None, action="ABSTAIN")


__all__ = ["Budget", "SigmaBudget"]
