# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""Python mirror of ``src/sigma/pipeline/engram.{h,c}``.

Pure Python; no numpy, no external deps.  Byte-for-byte FNV-1a-64
agreement with the C primitive (enforced by the parity test), so a
prompt hashed in Python can be looked up in a C-hosted table and vice
versa.  This is the cache the orchestrator (P8) uses to serve
previously-seen prompts in O(1).
"""

from __future__ import annotations

import dataclasses
from typing import Callable, Dict, Iterable, Optional, Tuple

_FNV_OFFSET = 1469598103934665603
_FNV_PRIME = 1099511628211
_U64_MASK = (1 << 64) - 1


def fnv1a_64(s: str) -> int:
    """FNV-1a-64, never returns 0 (0 is reserved as empty-slot sentinel)."""
    h = _FNV_OFFSET
    for b in s.encode("utf-8"):
        h ^= b
        h = (h * _FNV_PRIME) & _U64_MASK
    return h if h != 0 else 1


@dataclasses.dataclass
class EngramEntry:
    key_hash: int
    sigma_at_store: float
    value: object
    access_count: int = 0
    last_access_step: int = 0


@dataclasses.dataclass
class Engram:
    cap: int
    tau_trust: float
    long_ttl: int
    short_ttl: int
    entries: Dict[int, EngramEntry] = dataclasses.field(default_factory=dict)
    now_step: int = 0
    n_put: int = 0
    n_get_hit: int = 0
    n_get_miss: int = 0
    n_evict: int = 0
    n_aged: int = 0

    @classmethod
    def init(cls, cap: int, tau_trust: float,
             long_ttl: int, short_ttl: int) -> "Engram":
        if cap < 4 or (cap & (cap - 1)) != 0:
            raise ValueError("cap must be power of two ≥ 4")
        if not (0.0 <= tau_trust <= 1.0):
            raise ValueError("tau_trust out of range")
        if long_ttl < short_ttl:
            raise ValueError("long_ttl must be ≥ short_ttl")
        return cls(cap=cap, tau_trust=tau_trust,
                   long_ttl=long_ttl, short_ttl=short_ttl)

    def tick(self) -> None:
        self.now_step += 1

    def _score(self, e: EngramEntry) -> float:
        age = max(0, self.now_step - e.last_access_step)
        return e.sigma_at_store + 0.01 * age - 0.1 * e.access_count

    def _pick_victim(self) -> int:
        return max(self.entries.values(),
                   key=lambda e: self._score(e)).key_hash

    def put(self, key_hash: int, sigma: float, value: object
            ) -> Tuple[int, Optional[int]]:
        """Store/update entry.  Returns (rc, evicted_key_hash).

        rc == 0 → fresh insert or in-place update, no eviction.
        rc == 1 → insert triggered eviction; evicted_key_hash is the
                  victim's hash (caller should GC its value).
        Raises on argument errors. """
        if key_hash == 0:
            raise ValueError("key_hash cannot be 0 (sentinel)")
        if not (0.0 <= sigma <= 1.0):
            raise ValueError("sigma out of range")
        self.n_put += 1
        if key_hash in self.entries:
            e = self.entries[key_hash]
            e.sigma_at_store = sigma
            e.value = value
            e.last_access_step = self.now_step
            return 0, None
        evicted = None
        if len(self.entries) >= self.cap:
            evicted = self._pick_victim()
            del self.entries[evicted]
            self.n_evict += 1
        self.entries[key_hash] = EngramEntry(
            key_hash=key_hash, sigma_at_store=sigma, value=value,
            access_count=0, last_access_step=self.now_step,
        )
        return (1 if evicted is not None else 0), evicted

    def get(self, key_hash: int) -> Optional[EngramEntry]:
        e = self.entries.get(key_hash)
        if e is None:
            self.n_get_miss += 1
            return None
        e.access_count += 1
        e.last_access_step = self.now_step
        self.n_get_hit += 1
        return e

    def delete(self, key_hash: int) -> bool:
        return self.entries.pop(key_hash, None) is not None

    def age_sweep(self,
                  on_evict: Optional[Callable[[int, object], None]] = None
                  ) -> int:
        freed = 0
        doomed = []
        for e in self.entries.values():
            age = max(0, self.now_step - e.last_access_step)
            ttl = self.long_ttl if e.sigma_at_store <= self.tau_trust \
                else self.short_ttl
            if age > ttl:
                doomed.append(e.key_hash)
        for k in doomed:
            v = self.entries[k].value
            if on_evict is not None:
                on_evict(k, v)
            del self.entries[k]
            self.n_aged += 1
            freed += 1
        return freed
