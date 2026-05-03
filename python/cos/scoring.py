# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
"""Thin scoring facade: optional ``SigmaGate`` (LSD pickle) or quickstart scorer."""
from __future__ import annotations

from typing import Any, Optional, Protocol, Tuple, runtime_checkable


@runtime_checkable
class GateScorer(Protocol):
    def score(self, prompt: str, response: str) -> Tuple[float, str]: ...


class QuickscoreGate:
    """Deterministic demo σ + ACCEPT / RETHINK / ABSTAIN (no LSD pickle)."""

    def score(self, prompt: str, response: str) -> Tuple[float, str]:
        from cos.sigma_gate_quickscore import quickscore

        return quickscore(prompt, response)


def default_scorer(gate: Optional[Any] = None) -> GateScorer:
    """Return ``gate`` if set; otherwise :class:`QuickscoreGate`."""
    if gate is not None:
        return gate  # type: ignore[return-value]
    return QuickscoreGate()


def score_pair(
    prompt: str,
    response: str,
    *,
    gate: Optional[Any] = None,
) -> Tuple[float, str]:
    """Score ``(prompt, response)`` with ``gate`` or the quickstart scorer."""
    return default_scorer(gate).score(prompt, response)
