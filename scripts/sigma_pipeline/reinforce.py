# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""Python mirror of ``src/sigma/pipeline/reinforce.h``.

Pure functions.  No I/O, no global state.  Decision semantics MUST stay
byte-identical to the C primitive — a parity test in
``benchmarks/sigma_pipeline/check_sigma_reinforce_parity.py`` re-verifies
the two implementations agree on every canonical row and on an LCG grid.
"""

from __future__ import annotations

import enum
import math
from typing import Final


class Action(enum.IntEnum):
    ACCEPT = 0
    RETHINK = 1
    ABSTAIN = 2


MAX_ROUNDS: Final[int] = 3

RETHINK_SUFFIX: Final[str] = (
    "\n\nYour previous answer had low confidence. "
    "Reconsider from first principles, show each step, and "
    'if any step is uncertain, say "I do not know".'
)


def reinforce(sigma: float, tau_accept: float, tau_rethink: float) -> Action:
    """Return the three-state action for a single σ measurement.

    NaN or negative σ → ABSTAIN (safe default), matching the C primitive.
    A mis-configured pair (τ_accept > τ_rethink) also returns ABSTAIN.
    """
    try:
        s = float(sigma)
    except (TypeError, ValueError):
        return Action.ABSTAIN
    if math.isnan(s):
        return Action.ABSTAIN
    if not (s >= 0.0):
        return Action.ABSTAIN
    if tau_accept > tau_rethink:
        return Action.ABSTAIN
    if s < tau_accept:
        return Action.ACCEPT
    if s < tau_rethink:
        return Action.RETHINK
    return Action.ABSTAIN


def reinforce_round(
    sigma: float, tau_accept: float, tau_rethink: float, round_: int
) -> Action:
    """Round-aware policy: RETHINK at round ≥ MAX_ROUNDS-1 → ABSTAIN."""
    a = reinforce(sigma, tau_accept, tau_rethink)
    if a is Action.RETHINK and round_ >= MAX_ROUNDS - 1:
        return Action.ABSTAIN
    return a
