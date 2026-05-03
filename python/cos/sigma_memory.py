# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
v173 σ-memory: episodic + semantic stores with σ screening and toy consolidation (lab).

**Not brain-like memory:** linear scans; caps are soft in ``forget`` for demos.
"""
from __future__ import annotations

import hashlib
import re
import time
from typing import Any, Dict, List, MutableMapping


class SigmaMemorySystem:
    def __init__(self, gate: Any) -> None:
        self.gate = gate
        self.episodic: List[Dict[str, Any]] = []
        self.semantic: MutableMapping[str, Dict[str, Any]] = {}
        self.working: List[str] = []
        self.max_episodic = 100000
        self.max_semantic = 500000

    @staticmethod
    def now() -> float:
        return time.time()

    def store_episode(
        self,
        event: Any,
        context: str,
        emotional_valence: float = 0.0,
    ) -> Dict[str, Any]:
        sigma, verdict = self.gate.score(context or "", str(event))
        if str(verdict) == "ABSTAIN":
            return {"stored": False, "reason": "unreliable episode"}
        episode = {
            "event": event,
            "context": context,
            "emotional_valence": emotional_valence,
            "sigma": float(sigma),
            "timestamp": self.now(),
            "recall_count": 0,
            "consolidated": False,
        }
        self.episodic.append(episode)
        self.maybe_consolidate()
        return {"stored": True, "type": "episodic"}

    def store_semantic(self, fact: Any, source: str = "learned") -> Dict[str, Any]:
        sigma, verdict = self.gate.score("", str(fact))
        if str(verdict) == "ABSTAIN":
            return {"stored": False, "reason": "ABSTAIN semantic"}
        key = self.semantic_key(str(fact))
        if key in self.semantic:
            existing = self.semantic[key]
            existing["sigma"] = 0.1 * float(sigma) + 0.9 * float(existing["sigma"])
            existing["reinforced"] = int(existing.get("reinforced", 0)) + 1
        else:
            self.semantic[key] = {
                "fact": fact,
                "sigma": float(sigma),
                "source": source,
                "reinforced": 1,
                "timestamp": self.now(),
            }
        return {"stored": True, "type": "semantic"}

    def maybe_consolidate(self) -> None:
        if len(self.episodic) % 10 == 0:
            self.consolidate()

    def consolidate(self) -> Dict[str, Any]:
        themes: Dict[str, List[Dict[str, Any]]] = {}
        for ep in self.episodic:
            key = self.extract_theme(ep.get("event"))
            themes.setdefault(key, []).append(ep)
        consolidated = 0
        for theme, episodes in themes.items():
            if len(episodes) >= 3:
                self.store_semantic(f"Pattern: {theme} (from {len(episodes)} episodes)", source="consolidated")
                consolidated += 1
                for ep in episodes:
                    ep["consolidated"] = True
        return {"consolidated": consolidated}

    @staticmethod
    def semantic_key(fact: str) -> str:
        return hashlib.sha256(fact.encode("utf-8")).hexdigest()[:24]

    @staticmethod
    def extract_theme(event: Any) -> str:
        s = str(event).lower()
        s = re.sub(r"[^a-z0-9\s]+", "", s)
        words = [w for w in s.split() if len(w) > 3][:4]
        return "_".join(words) or "misc"

    @staticmethod
    def matches(query: str, text: Any) -> bool:
        q = query.lower()
        t = str(text).lower()
        return all(tok in t for tok in q.split() if len(tok) > 2)

    def forget(self, strategy: str = "sigma_weighted") -> Dict[str, Any]:
        before = len(self.episodic)
        if strategy == "sigma_weighted":
            self.episodic.sort(key=lambda e: float(e.get("sigma", 1.0)))
            if len(self.episodic) > self.max_episodic:
                self.episodic = self.episodic[: self.max_episodic]
        elif strategy == "age_weighted":
            horizon = 30 * 86400
            now = self.now()
            self.episodic = [
                e
                for e in self.episodic
                if not e.get("consolidated") or (now - float(e.get("timestamp", now))) < horizon
            ]
        return {"forgotten": before - len(self.episodic)}

    def recall(self, query: str, memory_type: str = "all") -> List[Dict[str, Any]]:
        results: List[Dict[str, Any]] = []
        if memory_type in ("episodic", "all"):
            for ep in self.episodic[-100:]:
                if self.matches(query, ep.get("event")):
                    ep["recall_count"] = int(ep.get("recall_count", 0)) + 1
                    row = {"type": "episodic", **ep}
                    results.append(row)
        if memory_type in ("semantic", "all"):
            for _key, fact in self.semantic.items():
                if self.matches(query, fact.get("fact")):
                    row = {"type": "semantic", **fact}
                    results.append(row)
        results.sort(key=lambda r: float(r.get("sigma", 1.0)))
        return results


__all__ = ["SigmaMemorySystem"]
