# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
σ-alerts for batch trace review (``cos observe --alerts``).

Kept separate from :mod:`cos.sigma_observe` so ingestion does not duplicate alert engines.
"""
from __future__ import annotations

from typing import TYPE_CHECKING, Any, Dict, List, Optional

if TYPE_CHECKING:
    from cos.sigma_observe import SigmaObserve


class SigmaAlert:
    """Lightweight drift / spike hints over a finished trace list."""

    def __init__(self, observe: Optional["SigmaObserve"] = None) -> None:
        self.observe = observe

    def check(self, traced: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
        alerts: List[Dict[str, Any]] = []
        if len(traced) < 2:
            return alerts
        sigmas: List[float] = []
        for t in traced:
            if not isinstance(t, dict):
                continue
            s = t.get("normalized_sigma", t.get("sigma"))
            try:
                sigmas.append(float(s))
            except (TypeError, ValueError):
                continue
        if len(sigmas) >= 3 and sigmas[-1] - sigmas[0] > 0.15:
            alerts.append(
                {
                    "name": "COHERENCE_DRIFT",
                    "severity": "warning",
                    "message": f"σ rose from {sigmas[0]:.3f} to {sigmas[-1]:.3f} over ingest window",
                },
            )
        if self.observe is not None and self.observe.alerts.active_count() > 0:
            alerts.extend(self.observe.alerts.active_alerts[-5:])
        return alerts


__all__ = ["SigmaAlert"]
