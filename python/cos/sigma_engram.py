# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
σ-filtered **engram** store: retain only ``ACCEPT`` trajectories for cheap retrieval.

**Harness-only:** in-memory list; swap for a vector index / disk log in production research.
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import Any, List, Optional, Tuple

from .sigma_gate_core import Verdict


@dataclass
class _Entry:
    query: str
    response: str
    sigma: float


class SigmaEngram:
    """Store ``(query, response, sigma)`` only when ``verdict == Verdict.ACCEPT``."""

    def __init__(self, *, max_entries: int = 10_000) -> None:
        self.max_entries = int(max_entries)
        self._rows: List[_Entry] = []

    def store(self, query: str, response: str, verdict: Verdict, sigma: float) -> None:
        if verdict != Verdict.ACCEPT:
            return
        q = (query or "").strip()
        r = (response or "").strip()
        if not q or not r:
            return
        self._rows.append(_Entry(q, r, float(sigma)))
        if len(self._rows) > self.max_entries:
            self._rows = self._rows[-self.max_entries :]

    def recall(self, query: str, tau: float = 0.3) -> List[Tuple[str, str, float]]:
        """
        Naive substring filter + ``sigma < tau`` (lower σ = calmer trace in this harness).
        Replace with embedding similarity when sentence-transformers is available.
        """
        q = (query or "").strip().lower()
        out: List[Tuple[str, str, float]] = []
        for e in self._rows:
            if e.sigma >= float(tau):
                continue
            if q and q in e.query.lower():
                out.append((e.query, e.response, e.sigma))
        return out

    def __len__(self) -> int:  # pragma: no cover
        return len(self._rows)


__all__ = ["SigmaEngram"]
