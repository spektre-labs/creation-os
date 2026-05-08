# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-gated KV cache — budgeted store; evict high-σ / low-hit / aged rows first.

Complements runtime engines (e.g. paged attention) at a **different layer**: this is a **lab**
policy object, not a vLLM plug-in. No SOTA t/s claims; see ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Dict, List, Optional, Tuple

from cos.sigma_gate import SigmaGate

__all__ = ["SigmaKVCache"]


@dataclass
class _KVEntry:
    key: str
    sigma: float
    value: Any
    seq: int
    hits: int = 0
    age: int = 0


def _eviction_score(e: _KVEntry) -> float:
    return (
        e.sigma * 0.7
        + (1.0 / max(e.hits + 1, 1)) * 0.2
        + min(e.age / 100.0, 1.0) * 0.1
    )


class SigmaKVCache:
    """KV rows with σ from explicit ``put`` or :meth:`add` (gate-scored), plus budget + merges."""

    def __init__(self, gate: Any = None, max_size: int = 256) -> None:
        self.gate = gate or SigmaGate()
        self.max_size = max(1, int(max_size))
        self._entries: List[_KVEntry] = []
        self._seq = 0
        self.evictions = 0
        self.total_ops = 0

    def __len__(self) -> int:
        return len(self._entries)

    def add(self, key: str, value: Any, context: str = "") -> None:
        """Insert after σ = :meth:`~cos.sigma_gate.SigmaGate.score` on the stringized pair."""
        σ, _ = self.gate.score(str(context or "kv_entry"), f"{key}={value}")
        self._seq += 1
        self._entries.append(_KVEntry(str(key), float(σ), value, self._seq, 0, 0))
        self.total_ops += 1
        if len(self._entries) > self.max_size:
            self._evict()

    def put(self, key: str, sigma: float, value: Any = None) -> None:
        """Legacy path: caller supplies σ (e.g. token-prefix probe)."""
        self._seq += 1
        self._entries.append(_KVEntry(str(key), float(sigma), value, self._seq, 0, 0))
        self.total_ops += 1
        self.enforce_budget(self.max_size)

    def _evict(self) -> None:
        """Remove the worst ~10% by σ / hits / age (lab heuristic)."""
        for entry in self._entries:
            entry.age += 1
        self._entries.sort(key=_eviction_score, reverse=True)
        n_remove = max(1, len(self._entries) // 10)
        self._entries = self._entries[n_remove:]
        self.evictions += n_remove

    def get(self, key: str) -> Optional[_KVEntry]:
        for entry in self._entries:
            if entry.key == key:
                entry.hits += 1
                return entry
        return None

    def get_value(self, key: str) -> Any:
        e = self.get(key)
        if e is None:
            return None
        return e.value

    def enforce_budget(self, max_size: Optional[int] = None) -> int:
        limit = int(max_size) if max_size is not None else self.max_size
        evicted = 0
        while len(self._entries) > limit:
            for entry in self._entries:
                entry.age += 1
            self._entries.sort(key=_eviction_score, reverse=True)
            self._entries.pop(0)
            evicted += 1
            self.evictions += 1
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

    def σ_weighted_attention(self, query: Any) -> List[Dict[str, Any]]:
        if not self._entries:
            return []
        weighted: List[Dict[str, Any]] = []
        for entry in self._entries:
            σ_query, _ = self.gate.score(str(query), str(entry.value))
            sq = float(σ_query)
            relevance = 1.0 - sq
            reliability = 1.0 - entry.sigma
            attention = relevance * reliability
            weighted.append(
                {
                    "key": entry.key,
                    "attention": round(float(attention), 4),
                    "relevance": round(float(relevance), 4),
                    "reliability": round(float(reliability), 4),
                }
            )
        weighted.sort(key=lambda w: float(w["attention"]), reverse=True)
        return weighted

    def compression_ratio(self) -> float:
        if self.total_ops == 0:
            return 1.0
        return round(len(self._entries) / float(self.total_ops), 4)

    def stats(self) -> Dict[str, Any]:
        if not self._entries:
            return {
                "size": 0,
                "entries": 0,
                "max_size": self.max_size,
                "evictions": self.evictions,
                "mean_sigma": 0.0,
                "compression": self.compression_ratio(),
                "total_ops": self.total_ops,
            }
        σ_values = [e.sigma for e in self._entries]
        ms = sum(σ_values) / len(σ_values)
        return {
            "size": len(self._entries),
            "entries": len(self._entries),
            "max_size": self.max_size,
            "utilization": round(len(self._entries) / self.max_size, 4),
            "avg_σ": round(float(ms), 4),
            "mean_sigma": round(float(ms), 6),
            "evictions": self.evictions,
            "compression": self.compression_ratio(),
            "total_ops": self.total_ops,
        }
