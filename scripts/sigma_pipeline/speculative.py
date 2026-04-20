# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""Python mirror of ``src/sigma/pipeline/speculative.h``."""

from __future__ import annotations

import enum
import math
from typing import Final


class Route(enum.IntEnum):
    LOCAL = 0
    ESCALATE = 1


SEGMENT_BOUNDARY_CHARS: Final[frozenset[str]] = frozenset(
    {".", "!", "?", "\n", "\r", ";"}
)


def route(sigma_peak: float, tau_escalate: float) -> Route:
    """LOCAL if σ_peak < τ_escalate, ESCALATE otherwise.

    NaN / negative σ_peak → ESCALATE (safe default), matching the C primitive.
    """
    try:
        s = float(sigma_peak)
    except (TypeError, ValueError):
        return Route.ESCALATE
    if math.isnan(s):
        return Route.ESCALATE
    if not (s >= 0.0):
        return Route.ESCALATE
    return Route.ESCALATE if s >= tau_escalate else Route.LOCAL


def peak_update(prev_peak: float, sigma_token: float) -> float:
    """Running max that ignores NaN / negative samples."""
    try:
        st = float(sigma_token)
    except (TypeError, ValueError):
        return prev_peak
    if math.isnan(st) or st < 0.0:
        return prev_peak
    return max(prev_peak, st)


def is_segment_boundary(ch: str) -> bool:
    if not ch:
        return False
    return ch[-1] in SEGMENT_BOUNDARY_CHARS


def cost_savings(
    n_total: int,
    n_escalated: int,
    eur_local_per_req: float,
    eur_api_per_req: float,
) -> float:
    """Fractional savings in [0, 1] vs API-only routing.

    Matches the formula in ``cos_sigma_speculative_cost_savings``:

        Cost(api_only) = n_total * eur_api
        Cost(hybrid)   = n_local * eur_local + n_escalated * (eur_local + eur_api)
        savings        = (api_only - hybrid) / api_only  clamped to [0, 1].
    """
    if n_total <= 0:
        return 0.0
    if n_escalated < 0 or n_escalated > n_total:
        return 0.0
    if not (eur_local_per_req >= 0.0):
        return 0.0
    if not (eur_api_per_req > 0.0):
        return 0.0
    n_local = n_total - n_escalated
    cost_api = n_total * eur_api_per_req
    cost_hybrid = n_local * eur_local_per_req + n_escalated * (
        eur_local_per_req + eur_api_per_req
    )
    s = (cost_api - cost_hybrid) / cost_api
    return max(0.0, min(1.0, s))
