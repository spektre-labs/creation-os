# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Three-tier σ-scored memory: working → episodic → semantic (+ procedural).

Each unit carries a gate σ; high σ means low trust (decay raises σ over time; forget drops
very high σ). Dream /consolidate promotes tiers; optional persistence under ``~/.cos/memory``.

Lab surface only — not a measured Mem0g benchmark replacement; see ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import hashlib
import json
import math
import time
from pathlib import Path
from typing import Any, Dict, List, Optional, Sequence

__all__ = ["Memory", "MemoryEntry", "SigmaMemory"]


def _verdict_str(verdict: object) -> str:
    if hasattr(verdict, "name"):
        return str(getattr(verdict, "name"))
    raw = str(verdict)
    if "." in raw:
        return raw.rsplit(".", 1)[-1]
    return raw


class Memory:
    """One σ-scored memory trace (working / episodic / semantic / procedural)."""

    def __init__(
        self,
        content: str,
        sigma: float,
        source: Optional[str] = None,
        memory_type: str = "episodic",
        *,
        mem_id: Optional[str] = None,
        ttl: Optional[float] = None,
    ) -> None:
        self.content = str(content)
        self.σ = float(sigma)
        self.source = source
        self.memory_type = str(memory_type)
        self.created = time.time()
        self.accessed = time.time()
        self.access_count = 0
        self.importance = 1.0 - self.σ
        self.ttl = ttl
        self.superseded_by: Optional[str] = None
        payload = f"{self.content}:{self.created}".encode("utf-8")
        self.id = mem_id or hashlib.sha256(payload).hexdigest()[:12]

    def access(self) -> None:
        self.accessed = time.time()
        self.access_count += 1

    def age_days(self) -> float:
        return (time.time() - self.created) / 86400.0

    def is_expired(self) -> bool:
        if self.ttl is None:
            return False
        return (time.time() - self.created) > float(self.ttl)

    @property
    def sigma(self) -> float:
        """ASCII alias for σ (compat with older call sites)."""
        return self.σ

    @sigma.setter
    def sigma(self, v: float) -> None:
        self.σ = float(v)
        self.importance = 1.0 - self.σ

    @property
    def last_accessed(self) -> float:
        return self.accessed

    @last_accessed.setter
    def last_accessed(self, v: float) -> None:
        self.accessed = float(v)

    @property
    def relevance(self) -> float:
        """Maps to importance for legacy ``dream`` / prune paths."""
        return self.importance

    @relevance.setter
    def relevance(self, v: float) -> None:
        self.importance = float(v)

    def decay(self, rate: float = 0.01) -> None:
        age = time.time() - self.last_accessed
        self.importance *= math.exp(-rate * age / 3600.0)

    def to_dict(self) -> Dict[str, Any]:
        return {
            "id": self.id,
            "content": self.content,
            "σ": round(self.σ, 4),
            "sigma": round(self.σ, 4),
            "type": self.memory_type,
            "source": self.source,
            "created": self.created,
            "accessed": self.accessed,
            "access_count": self.access_count,
            "importance": round(self.importance, 4),
        }

    def __repr__(self) -> str:
        return f"Memory({self.memory_type!r}:{self.content[:30]!r}, σ={self.σ:.3f})"


# Backward-compatible name for older imports / prose
MemoryEntry = Memory


class SigmaMemory:
    """σ-gated three-tier store + procedural tier; recall, consolidation, decay, forget."""

    def __init__(
        self,
        gate: Any = None,
        graph: Any = None,
        persist_dir: Optional[Union[str, Path]] = None,
        *,
        write_threshold: float = 0.5,
        max_entries: int = 10000,
        decay_rate: float = 0.01,
    ) -> None:
        from cos.sigma_gate import SigmaGate

        self.gate = gate or SigmaGate()
        self.graph = graph
        self.persist_dir = Path(persist_dir or "~/.cos/memory").expanduser()
        self.persist_dir.mkdir(parents=True, exist_ok=True)
        self.write_threshold = float(write_threshold)
        self.max_entries = int(max_entries)
        self.decay_rate = float(decay_rate)
        self.write_log: List[Dict[str, Any]] = []

        self.working: List[Memory] = []
        self.episodic: List[Memory] = []
        self.semantic: List[Memory] = []
        self.procedural: List[Memory] = []

        self._load()

    @property
    def entries(self) -> List[Memory]:
        """Flat view of all tiers (tests / legacy introspection)."""
        return self.working + self.episodic + self.semantic + self.procedural

    def _tier(self, memory_type: str) -> List[Memory]:
        t = str(memory_type).lower()
        if t == "working":
            return self.working
        if t == "episodic":
            return self.episodic
        if t == "semantic":
            return self.semantic
        if t == "procedural":
            return self.procedural
        return self.episodic

    def store(
        self,
        content: str,
        context: str = "",
        memory_type: str = "episodic",
    ) -> Dict[str, Any]:
        """Score (context, content) with the gate and append to the named tier."""
        ctx = context or "memory store"
        sigma, verdict = self.gate.score(ctx, content)
        sigma = float(sigma)
        vn = _verdict_str(verdict)
        mem = Memory(content, sigma, source=context or None, memory_type=memory_type)
        tier = self._tier(memory_type)
        tier.append(mem)
        self._apply_ttl_defaults(mem)
        if self.graph is not None and vn != "ABSTAIN":
            try:
                self.graph.add("memory", "contains", content[:100], sigma=sigma)
            except Exception:
                pass
        self._prune_all()
        self._save()
        return {"σ": sigma, "sigma": sigma, "verdict": vn, "type": memory_type, "id": mem.id}

    def _apply_ttl_defaults(self, mem: Memory) -> None:
        if mem.ttl is not None:
            return
        if mem.memory_type == "episodic":
            mem.ttl = 86400.0 * 30.0
        elif mem.memory_type == "procedural":
            mem.ttl = None

    def write(
        self,
        content: str,
        memory_type: str = "semantic",
        source: Optional[str] = None,
        sigma: Optional[float] = None,
        ttl: Optional[float] = None,
        force: bool = False,
    ) -> Dict[str, Any]:
        """Legacy σ-gated write with conflict checks (semantic overlap)."""
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
                },
            )
            return {"written": False, "reason": "sigma_too_high", "sigma": sigma}

        conflicts = self._find_conflicts(content, memory_type)
        if conflicts and not force:
            if any(old.σ < sigma for old in conflicts):
                self.write_log.append(
                    {
                        "action": "CONFLICT_BLOCKED",
                        "content": content[:50],
                        "sigma": round(sigma, 4),
                        "conflicting_with": conflicts[0].content[:50],
                    },
                )
                return {
                    "written": False,
                    "reason": "conflict_higher_sigma",
                    "conflicting_with": conflicts[0].content[:50],
                }
            for old in conflicts:
                old.superseded_by = content

        mem = Memory(content, sigma, source=source, memory_type=memory_type, ttl=ttl)
        self._apply_ttl_defaults(mem)
        self._tier(memory_type).append(mem)
        self._prune_all()
        self.write_log.append(
            {"action": "WRITTEN", "id": mem.id, "type": memory_type, "sigma": round(sigma, 4)},
        )
        self._save()
        return {"written": True, "id": mem.id, "sigma": sigma}

    def recall(
        self,
        query: str,
        memory_type: Optional[str] = None,
        top_k: int = 5,
        max_σ: float = 0.8,
        memory_types: Optional[Sequence[str]] = None,
    ) -> List[Dict[str, Any]]:
        """Token-overlap recall; drop memories with σ > ``max_σ`` (too unreliable)."""
        types: List[str]
        if memory_types is not None:
            types = list(memory_types)
        elif memory_type:
            types = [memory_type]
        else:
            types = ["working", "episodic", "semantic", "procedural"]

        candidates: List[tuple[float, Memory]] = []
        for tier_name in types:
            tier = getattr(self, tier_name, [])
            for mem in tier:
                if mem.superseded_by is not None or mem.is_expired():
                    continue
                if mem.σ > float(max_σ):
                    continue
                mem.decay(self.decay_rate)
                relevance = self._relevance(query, mem.content)
                candidates.append((relevance, mem))

        candidates.sort(key=lambda x: x[0], reverse=True)
        out: List[Dict[str, Any]] = []
        for rel, mem in candidates[: int(top_k)]:
            mem.access()
            row = {**mem.to_dict(), "relevance": round(rel, 4), "score": round(rel, 4)}
            out.append(row)
        return out

    def consolidate(self, *, mode: str = "tiers", **kwargs: Any) -> Dict[str, Any]:
        """``mode='tiers'``: working→episodic; frequent episodic→semantic.

        ``mode='semantic_dedup'``: merge overlapping high-access semantic rows (legacy).
        """
        if mode == "semantic_dedup":
            return self._merge_semantic(**kwargs)
        return self._consolidate_tiers()

    def _consolidate_tiers(self) -> Dict[str, Any]:
        moved = 0
        for mem in list(self.working):
            mem.memory_type = "episodic"
            self.episodic.append(mem)
            moved += 1
        self.working.clear()

        content_counts: Dict[str, int] = {}
        for mem in self.episodic:
            key = mem.content[:50]
            content_counts[key] = content_counts.get(key, 0) + 1

        promoted = 0
        promoted_keys: set[str] = set()
        for key, count in content_counts.items():
            if count < 3:
                continue
            group = [m for m in self.episodic if m.content[:50] == key]
            if not group:
                continue
            best = min(group, key=lambda m: m.σ)
            best.memory_type = "semantic"
            self.semantic.append(best)
            promoted_keys.add(key)
            promoted += 1

        if promoted_keys:
            self.episodic = [m for m in self.episodic if m.content[:50] not in promoted_keys]

        self._save()
        return {"working_to_episodic": moved, "episodic_to_semantic": promoted, "mode": "tiers"}

    def _merge_semantic(
        self,
        *,
        memory_type: str = "semantic",
        min_access: int = 2,
        overlap_threshold: float = 0.55,
    ) -> Dict[str, Any]:
        """Collapse near-duplicate **semantic** rows (legacy lab helper)."""
        if memory_type != "semantic":
            return {"merged": 0, "note": "only semantic consolidation implemented"}
        removed = 0
        kept: List[Memory] = []
        for entry in sorted(self.semantic, key=lambda e: e.access_count, reverse=True):
            if entry.superseded_by is not None:
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
        self.semantic = kept
        self._save()
        return {"merged": removed, "remaining": len(self.entries), "mode": "semantic_dedup"}

    def decay(
        self,
        max_age_days: float = 30.0,
        decay_rate: Optional[float] = None,
    ) -> int:
        """Raise σ for old, seldom-accessed episodic/semantic entries."""
        rate = float(decay_rate if decay_rate is not None else self.decay_rate)
        decayed = 0
        for tier in (self.episodic, self.semantic):
            for mem in tier:
                age = mem.age_days()
                if age <= max_age_days:
                    continue
                days_over = age - max_age_days
                freq_factor = 1.0 / (1 + mem.access_count)
                mem.σ = min(1.0, mem.σ + rate * days_over * freq_factor)
                mem.importance = 1.0 - mem.σ
                decayed += 1
        if decayed:
            self._save()
        return decayed

    def forget(
        self,
        entry_id: Optional[str] = None,
        memory_type: Optional[str] = None,
        older_than: Optional[float] = None,
        σ_threshold: Optional[float] = None,
    ) -> Dict[str, int]:
        if all(x is None for x in (entry_id, memory_type, older_than, σ_threshold)):
            n = self._forget_high_sigma(0.95)
            return {"forgotten": n}

        if σ_threshold is not None and entry_id is None and memory_type is None and older_than is None:
            n = self._forget_high_sigma(float(σ_threshold))
            return {"forgotten": n}

        before = len(self.entries)
        if entry_id:
            for name in ("working", "episodic", "semantic", "procedural"):
                tier: List[Memory] = getattr(self, name)
                setattr(self, name, [m for m in tier if m.id != entry_id])
        elif memory_type:
            mt = str(memory_type)
            for name in ("working", "episodic", "semantic", "procedural"):
                tier = getattr(self, name)
                setattr(self, name, [m for m in tier if m.memory_type != mt])
        elif older_than is not None:
            cutoff = time.time() - float(older_than)
            for name in ("working", "episodic", "semantic", "procedural"):
                tier = getattr(self, name)
                setattr(self, name, [m for m in tier if m.created > cutoff])
        self._save()
        return {"forgotten": before - len(self.entries)}

    def _forget_high_sigma(self, σ_threshold: float) -> int:
        forgotten = 0
        for tier_name in ("episodic", "semantic"):
            tier: List[Memory] = getattr(self, tier_name)
            before = len(tier)
            new_tier = [m for m in tier if m.σ < σ_threshold]
            setattr(self, tier_name, new_tier)
            forgotten += before - len(new_tier)
        if forgotten:
            self._save()
        return forgotten

    def stats(self) -> Dict[str, Any]:
        by_type: Dict[str, int] = {}
        for m in self.entries:
            by_type[m.memory_type] = by_type.get(m.memory_type, 0) + 1
        n = max(len(self.entries), 1)
        return {
            "working": len(self.working),
            "episodic": len(self.episodic),
            "semantic": len(self.semantic),
            "procedural": len(self.procedural),
            "total": len(self.entries),
            "avg_σ": self._avg_sigma(),
            "avg_sigma": self._avg_sigma(),
            "by_type": by_type,
            "avg_relevance": sum(m.importance for m in self.entries) / n,
            "writes_blocked": sum(1 for row in self.write_log if row.get("action") == "BLOCKED"),
            "conflicts_blocked": sum(
                1 for row in self.write_log if row.get("action") == "CONFLICT_BLOCKED"
            ),
        }

    def _avg_sigma(self) -> float:
        if not self.entries:
            return 0.0
        return round(sum(m.σ for m in self.entries) / len(self.entries), 4)

    def long_term_store(
        self,
        content: str,
        *,
        source: Optional[str] = None,
        sigma: Optional[float] = None,
        ttl: Optional[float] = None,
        force: bool = False,
    ) -> Dict[str, Any]:
        return self.write(
            content,
            memory_type="semantic",
            source=source,
            sigma=sigma,
            ttl=ttl,
            force=force,
        )

    def dream(self, *, n_samples: int = 4, prefix: str = "dream replay") -> Dict[str, Any]:
        """Replay weakest-importance traces through the gate (offline lab)."""
        if not self.entries:
            return {"replayed": 0, "traces": []}
        weak = sorted(self.entries, key=lambda e: e.importance)[: max(1, int(n_samples))]
        traces: List[Dict[str, Any]] = []
        for e in weak:
            s, v = self.gate.score(prefix, e.content[:800])
            traces.append({"id": e.id, "sigma": round(float(s), 4), "verdict": str(v)})
        return {"replayed": len(traces), "traces": traces}

    def _relevance(self, query: str, content: str) -> float:
        q_words = set(query.lower().split())
        c_words = set(content.lower().split())
        if not q_words:
            return 0.0
        return len(q_words & c_words) / len(q_words)

    def _find_conflicts(self, content: str, memory_type: str) -> List[Memory]:
        if memory_type != "semantic":
            return []
        content_words = set(content.lower().split())
        conflicts: List[Memory] = []
        for entry in self.semantic:
            if entry.superseded_by is not None:
                continue
            entry_words = set(entry.content.lower().split())
            union = content_words | entry_words
            overlap = len(content_words & entry_words) / max(len(union), 1)
            if overlap > 0.5 and content != entry.content:
                conflicts.append(entry)
        return conflicts

    def _prune_all(self) -> None:
        for name in ("working", "episodic", "semantic", "procedural"):
            tier: List[Memory] = getattr(self, name)
            tier[:] = [m for m in tier if not m.is_expired()]
        total = len(self.entries)
        if total <= self.max_entries:
            return
        flat = sorted(self.entries, key=lambda m: (m.σ, -m.importance), reverse=True)
        drop = total - self.max_entries
        to_drop = {m.id for m in flat[:drop]}
        for name in ("working", "episodic", "semantic", "procedural"):
            tier = getattr(self, name)
            setattr(self, name, [m for m in tier if m.id not in to_drop])

    def _save(self) -> None:
        data = {
            "episodic": [m.to_dict() for m in self.episodic],
            "semantic": [m.to_dict() for m in self.semantic],
            "procedural": [m.to_dict() for m in self.procedural],
        }
        path = self.persist_dir / "memories.json"
        path.write_text(json.dumps(data, indent=2), encoding="utf-8")

    def _load(self) -> None:
        path = self.persist_dir / "memories.json"
        if not path.is_file():
            return
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except json.JSONDecodeError:
            return
        for tier_name in ("episodic", "semantic", "procedural"):
            for d in data.get(tier_name, []) or []:
                try:
                    sig = float(d.get("σ", d.get("sigma", 0.0)))
                    mem = Memory(
                        str(d["content"]),
                        sig,
                        source=d.get("source"),
                        memory_type=str(d.get("type", tier_name)),
                        mem_id=d.get("id"),
                    )
                    mem.created = float(d.get("created", time.time()))
                    mem.accessed = float(d.get("accessed", mem.created))
                    mem.access_count = int(d.get("access_count", 0))
                    mem.importance = float(d.get("importance", 1.0 - sig))
                    getattr(self, tier_name).append(mem)
                except (KeyError, TypeError, ValueError):
                    continue
