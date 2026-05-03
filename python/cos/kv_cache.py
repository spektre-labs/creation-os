# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-KV cache — budgeted store that prefers evicting high-σ entries first.

Pairs with :mod:`cos.recursion` only at the conceptual level (selective KV). No H2O
replay numbers; see ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Dict, List, Optional, Tuple

__all__ = ["SigmaKVCache"]


@dataclass
class _KVEntry:
    key: str
    sigma: float
    value: Any
    seq: int


class SigmaKVCache:
    """Budget KV with σ-priority eviction and light duplicate merge."""

    def __init__(self, *, max_size: int = 256) -> None:
        self.max_size = max(1, int(max_size))
        self._entries: List[_KVEntry] = []
        self._seq = 0

    def __len__(self) -> int:
        return len(self._entries)

    def put(self, key: str, sigma: float, value: Any = None) -> None:
        self._seq += 1
        self._entries.append(
            _KVEntry(str(key), float(sigma), value, self._seq),
        )
        self.enforce_budget(self.max_size)

    def enforce_budget(self, max_size: Optional[int] = None) -> int:
        """Drop highest-σ rows first until within budget; returns evicted count."""
        limit = int(max_size) if max_size is not None else self.max_size
        evicted = 0
        while len(self._entries) > limit:
            worst = max(range(len(self._entries)), key=lambda i: self._entries[i].sigma)
            self._entries.pop(worst)
            evicted += 1
        return evicted

    def mean_sigma(self) -> float:
        if not self._entries:
            return 0.0
        return sum(e.sigma for e in self._entries) / len(self._entries)

    def sliding_window(
        self,
        window: int,
        *,
        keep_below: float = 0.35,
    ) -> None:
        """Keep last ``window`` rows, but retain older rows if σ is low (sticky)."""
        if window < 1 or len(self._entries) <= window:
            return
        self._entries.sort(key=lambda e: e.seq)
        head = self._entries[:-window]
        tail = self._entries[-window:]
        sticky = [e for e in head if e.sigma <= keep_below]
        merged = sticky + tail
        merged.sort(key=lambda e: e.seq)
        self._entries = merged
        self.enforce_budget(self.max_size)

    def merge_similar(self, *, sigma_quant: float = 0.05) -> int:
        """Bucket by quantized σ; keep one representative per bucket (count merge)."""
        if not self._entries:
            return 0
        q = float(sigma_quant)
        buckets: Dict[Tuple[str, int], _KVEntry] = {}
        before = len(self._entries)
        for e in self._entries:
            bkey = (e.key.split(":", 1)[0], int(round(e.sigma / q)))
            if bkey not in buckets or e.sigma < buckets[bkey].sigma:
                buckets[bkey] = e
        self._entries = list(buckets.values())
        return before - len(self._entries)

    def get(self, key: str) -> Optional[_KVEntry]:
        for e in self._entries:
            if e.key == key:
                return e
        return None

    def stats(self) -> Dict[str, Any]:
        return {
            "entries": len(self._entries),
            "max_size": self.max_size,
            "mean_sigma": round(self.mean_sigma(), 6),
        }
