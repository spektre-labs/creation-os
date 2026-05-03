# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-memory — typed store (episodic / semantic / procedural) with σ-gated writes.

Lab implementation: conflict cue via lexical overlap, TTL, decay-on-recall, and
bounded growth. Not a vector database and not a measured end-to-end benchmark;
do not cite unpublished percentage drops in committed prose — see
``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import hashlib
import math
import time
from typing import Any, Dict, List, Optional


class MemoryEntry:
    """One stored memory trace."""

    def __init__(
        self,
        content: str,
        memory_type: str,
        source: Optional[str] = None,
        sigma: float = 0.0,
        ttl: Optional[float] = None,
    ) -> None:
        self.id = hashlib.sha256(f"{content}{time.time()}".encode()).hexdigest()[:12]
        self.content = str(content)
        self.memory_type = str(memory_type)
        self.source = source
        self.sigma = float(sigma)
        self.created = time.time()
        self.last_accessed = self.created
        self.access_count = 0
        self.relevance = 1.0
        self.ttl = ttl
        self.superseded_by: Optional[str] = None

    def access(self) -> None:
        self.last_accessed = time.time()
        self.access_count += 1
        self.relevance = min(1.0, self.relevance + 0.1)

    def decay(self, rate: float = 0.01) -> None:
        age = time.time() - self.last_accessed
        self.relevance *= math.exp(-rate * age / 3600.0)

    def is_expired(self) -> bool:
        if self.ttl is None:
            return False
        return (time.time() - self.created) > float(self.ttl)

    def __repr__(self) -> str:
        return f"Mem({self.memory_type}:{self.content[:30]!r}, rel={self.relevance:.2f})"


class SigmaMemory:
    """σ-gated writes, recall with overlap + recency + relevance, explicit forget."""

    def __init__(
        self,
        gate: Any = None,
        write_threshold: float = 0.5,
        max_entries: int = 10000,
        decay_rate: float = 0.01,
    ) -> None:
        from cos.sigma_gate import SigmaGate

        self.gate = gate or SigmaGate()
        self.write_threshold = float(write_threshold)
        self.max_entries = int(max_entries)
        self.decay_rate = float(decay_rate)
        self.entries: List[MemoryEntry] = []
        self.write_log: List[Dict[str, Any]] = []

    def write(
        self,
        content: str,
        memory_type: str = "semantic",
        source: Optional[str] = None,
        sigma: Optional[float] = None,
        ttl: Optional[float] = None,
        force: bool = False,
    ) -> Dict[str, Any]:
        if sigma is None:
            sigma, _ = self.gate.score("memory write", content)
        sigma = float(sigma)

        if not force and sigma > self.write_threshold:
            self.write_log.append(
                {
                    "action": "BLOCKED",
                    "content": content[:50],
                    "sigma": round(sigma, 4),
                    "reason": "sigma too high",
                }
            )
            return {"written": False, "reason": "sigma_too_high", "sigma": sigma}

        conflicts = self._find_conflicts(content, memory_type)
        if conflicts and not force:
            if any(old.sigma < sigma for old in conflicts):
                self.write_log.append(
                    {
                        "action": "CONFLICT_BLOCKED",
                        "content": content[:50],
                        "sigma": round(sigma, 4),
                        "conflicting_with": conflicts[0].content[:50],
                    }
                )
                return {
                    "written": False,
                    "reason": "conflict_higher_sigma",
                    "conflicting_with": conflicts[0].content[:50],
                }
            for old in conflicts:
                old.superseded_by = content

        if ttl is None:
            if memory_type == "episodic":
                ttl = 86400.0 * 30.0
            elif memory_type == "procedural":
                ttl = None

        entry = MemoryEntry(content, memory_type, source, sigma, ttl)
        self.entries.append(entry)
        if len(self.entries) > self.max_entries:
            self._prune()

        self.write_log.append(
            {
                "action": "WRITTEN",
                "id": entry.id,
                "type": memory_type,
                "sigma": round(sigma, 4),
            }
        )
        return {"written": True, "id": entry.id, "sigma": sigma}

    def recall(
        self,
        query: str,
        memory_type: Optional[str] = None,
        top_k: int = 5,
    ) -> List[Dict[str, Any]]:
        for entry in self.entries:
            entry.decay(self.decay_rate)

        candidates: List[MemoryEntry] = self.entries
        if memory_type:
            candidates = [e for e in candidates if e.memory_type == memory_type]
        candidates = [e for e in candidates if not e.is_expired() and e.superseded_by is None]

        query_words = set(query.lower().split())
        scored: List[tuple[MemoryEntry, float]] = []

        for entry in candidates:
            content_words = set(entry.content.lower().split())
            if query_words and content_words:
                overlap = len(query_words & content_words) / len(query_words | content_words)
            else:
                overlap = 0.0

            age_hours = (time.time() - entry.last_accessed) / 3600.0
            recency = math.exp(-0.01 * age_hours)
            freq_bonus = min(0.2, entry.access_count * 0.02)
            score = 0.5 * overlap + 0.2 * recency + 0.2 * entry.relevance + 0.1 * freq_bonus
            scored.append((entry, score))

        scored.sort(key=lambda x: x[1], reverse=True)
        results = scored[: int(top_k)]

        for entry, _ in results:
            entry.access()

        return [
            {
                "id": entry.id,
                "content": entry.content,
                "type": entry.memory_type,
                "score": round(score, 4),
                "sigma": entry.sigma,
                "access_count": entry.access_count,
            }
            for entry, score in results
        ]

    def forget(
        self,
        entry_id: Optional[str] = None,
        memory_type: Optional[str] = None,
        older_than: Optional[float] = None,
    ) -> Dict[str, int]:
        before = len(self.entries)
        if entry_id:
            self.entries = [e for e in self.entries if e.id != entry_id]
        elif memory_type:
            self.entries = [e for e in self.entries if e.memory_type != memory_type]
        elif older_than is not None:
            cutoff = time.time() - float(older_than)
            self.entries = [e for e in self.entries if e.created > cutoff]
        return {"forgotten": before - len(self.entries)}

    def _find_conflicts(self, content: str, memory_type: str) -> List[MemoryEntry]:
        if memory_type != "semantic":
            return []
        content_words = set(content.lower().split())
        conflicts: List[MemoryEntry] = []
        for entry in self.entries:
            if entry.memory_type != "semantic" or entry.superseded_by is not None:
                continue
            entry_words = set(entry.content.lower().split())
            union = content_words | entry_words
            overlap = len(content_words & entry_words) / max(len(union), 1)
            if overlap > 0.5 and content != entry.content:
                conflicts.append(entry)
        return conflicts

    def _prune(self) -> None:
        self.entries = [e for e in self.entries if not e.is_expired()]
        self.entries.sort(key=lambda e: e.relevance, reverse=True)
        self.entries = self.entries[: self.max_entries]

    def stats(self) -> Dict[str, Any]:
        types: Dict[str, int] = {}
        for e in self.entries:
            types[e.memory_type] = types.get(e.memory_type, 0) + 1
        n = max(len(self.entries), 1)
        return {
            "total": len(self.entries),
            "by_type": types,
            "avg_relevance": sum(e.relevance for e in self.entries) / n,
            "avg_sigma": sum(e.sigma for e in self.entries) / n,
            "writes_blocked": sum(1 for row in self.write_log if row.get("action") == "BLOCKED"),
            "conflicts_blocked": sum(
                1 for row in self.write_log if row.get("action") == "CONFLICT_BLOCKED"
            ),
        }

    def long_term_store(
        self,
        content: str,
        *,
        source: Optional[str] = None,
        sigma: Optional[float] = None,
        ttl: Optional[float] = None,
        force: bool = False,
    ) -> Dict[str, Any]:
        """Convenience wrapper: semantic store path."""
        return self.write(
            content,
            memory_type="semantic",
            source=source,
            sigma=sigma,
            ttl=ttl,
            force=force,
        )

    def consolidate(
        self,
        *,
        memory_type: str = "semantic",
        min_access: int = 2,
        overlap_threshold: float = 0.55,
    ) -> Dict[str, Any]:
        """Merge highly overlapping semantic entries that were accessed enough to matter."""
        if memory_type != "semantic":
            return {"merged": 0, "note": "only semantic consolidation implemented"}
        removed = 0
        kept: List[MemoryEntry] = []
        for entry in sorted(self.entries, key=lambda e: e.access_count, reverse=True):
            if entry.memory_type != "semantic" or entry.superseded_by is not None:
                kept.append(entry)
                continue
            if entry.access_count < int(min_access):
                kept.append(entry)
                continue
            dup = False
            ew = set(entry.content.lower().split())
            for other in kept:
                if other is entry or other.memory_type != "semantic":
                    continue
                ow = set(other.content.lower().split())
                u = ew | ow
                if not u:
                    continue
                if len(ew & ow) / len(u) >= float(overlap_threshold):
                    dup = True
                    other.access_count += entry.access_count
                    removed += 1
                    break
            if not dup:
                kept.append(entry)
        self.entries = kept
        return {"merged": removed, "remaining": len(self.entries)}

    def dream(self, *, n_samples: int = 4, prefix: str = "dream replay") -> Dict[str, Any]:
        """Replay low-relevance traces through the gate (offline consolidation lab)."""
        if not self.entries:
            return {"replayed": 0, "traces": []}
        weak = sorted(self.entries, key=lambda e: e.relevance)[: max(1, int(n_samples))]
        traces: List[Dict[str, Any]] = []
        for e in weak:
            s, v = self.gate.score(prefix, e.content[:800])
            traces.append(
                {
                    "id": e.id,
                    "sigma": round(float(s), 4),
                    "verdict": str(v),
                }
            )
        return {"replayed": len(traces), "traces": traces}


__all__ = ["MemoryEntry", "SigmaMemory"]
