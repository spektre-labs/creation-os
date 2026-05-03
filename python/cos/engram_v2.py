# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
Engram **v2**: σ-filtered hierarchical memory, knowledge graph, staleness, consolidation (lab).

- **Store:** only ``Verdict.ACCEPT`` (or string ``\"ACCEPT\"``).
- **Recall:** entries with **effective σ < τ** (low σ = calmer / more trusted in this harness).

Ω-loop hooks: **REMEMBER** (store/recall), **CONSOLIDATE** (``consolidate()``).

See ``docs/CLAIM_DISCIPLINE.md`` — no vendor memory product equivalence claims.
"""
from __future__ import annotations

import hashlib
import json
import re
import time
import uuid
from collections import deque
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional, Sequence, Tuple, Union

from cos.engram_graph import KnowledgeGraph
from cos.sigma_gate_core import Verdict

VerdictLike = Union[Verdict, str]


def _verdict_name(v: VerdictLike) -> str:
    if isinstance(v, Verdict):
        return v.name
    return str(v).strip().upper()


def _norm_tokens(text: str) -> List[str]:
    return [t for t in re.findall(r"[a-z0-9]+", text.lower()) if len(t) > 2]


def _fingerprint(content: str) -> str:
    n = re.sub(r"\s+", " ", str(content).strip().lower())
    return hashlib.sha256(n.encode("utf-8")).hexdigest()[:24]


def _parse_ts(memory: Dict[str, Any]) -> datetime:
    ts = memory.get("timestamp")
    if isinstance(ts, datetime):
        return ts if ts.tzinfo else ts.replace(tzinfo=timezone.utc)
    if isinstance(ts, str):
        return datetime.fromisoformat(ts.replace("Z", "+00:00"))
    if isinstance(ts, (int, float)):
        return datetime.fromtimestamp(float(ts), tz=timezone.utc)
    return datetime.now(timezone.utc)


@dataclass
class WorkingMemory:
    max_items: int = 32
    ttl_seconds: float = 300.0
    _items: deque = field(default_factory=deque)
    _clock: Callable[[], float] = field(default_factory=lambda: time.time)

    def add(self, memory: Dict[str, Any]) -> None:
        now = float(self._clock())
        self._purge(now)
        self._items.append((now, dict(memory)))
        while len(self._items) > self.max_items:
            self._items.popleft()

    def _purge(self, now: float) -> None:
        while self._items and (now - self._items[0][0]) > self.ttl_seconds:
            self._items.popleft()

    def snapshot(self) -> List[Dict[str, Any]]:
        now = float(self._clock())
        self._purge(now)
        return [dict(m) for _, m in self._items]

    def remove_by_id(self, mid: str) -> None:
        self._items = deque((t, m) for t, m in self._items if m.get("id") != mid)


@dataclass
class EpisodicMemory:
    _items: Dict[str, Dict[str, Any]] = field(default_factory=dict)

    def add(self, memory: Dict[str, Any]) -> str:
        mid = str(memory.get("id") or uuid.uuid4().hex)
        row = dict(memory)
        row["id"] = mid
        self._items[mid] = row
        return mid

    def get(self, mid: str) -> Optional[Dict[str, Any]]:
        return self._items.get(mid)

    def remove(self, mid: str) -> None:
        self._items.pop(mid, None)

    def bump_access(self, mid: str) -> None:
        m = self._items.get(mid)
        if m:
            m["access_count"] = int(m.get("access_count", 0)) + 1

    def all(self) -> List[Dict[str, Any]]:
        return list(self._items.values())

    def get_frequent(self, *, min_count: int = 3) -> List[Dict[str, Any]]:
        buckets: Dict[str, List[Dict[str, Any]]] = {}
        for m in self._items.values():
            key = _fingerprint(str(m.get("content", "")))
            buckets.setdefault(key, []).append(dict(m))
        out: List[Dict[str, Any]] = []
        for instances in buckets.values():
            if len(instances) >= min_count:
                out.append({"instances": instances, "fingerprint": _fingerprint(str(instances[0].get("content", "")))})
        return out


@dataclass
class SemanticMemory:
    _items: List[Dict[str, Any]] = field(default_factory=list)

    def add(self, memory: Dict[str, Any]) -> str:
        mid = str(memory.get("id") or uuid.uuid4().hex)
        row = dict(memory)
        row["id"] = mid
        self._items.append(row)
        return mid

    def all(self) -> List[Dict[str, Any]]:
        return list(self._items)


@dataclass
class LongTermMemory:
    _items: List[Dict[str, Any]] = field(default_factory=list)

    def add(self, memory: Dict[str, Any]) -> str:
        mid = str(memory.get("id") or uuid.uuid4().hex)
        row = dict(memory)
        row["id"] = mid
        self._items.append(row)
        return mid

    def all(self) -> List[Dict[str, Any]]:
        return list(self._items)


class EngramV2:
    """σ-gated hierarchical store + ``KnowledgeGraph`` + staleness (lab)."""

    def __init__(
        self,
        gate: Any = None,
        graph_backend: str = "sqlite",
        *,
        graph: Optional[KnowledgeGraph] = None,
        now_fn: Optional[Callable[[], datetime]] = None,
    ) -> None:
        self.gate = gate
        self.graph = graph or KnowledgeGraph(backend=graph_backend)
        self._now_fn = now_fn or (lambda: datetime.now(timezone.utc))
        self.layers: Dict[str, Any] = {
            "working": WorkingMemory(),
            "episodic": EpisodicMemory(),
            "semantic": SemanticMemory(),
            "long_term": LongTermMemory(),
        }

    def now(self) -> datetime:
        return self._now_fn()

    def store(
        self,
        content: str,
        source: str,
        sigma: float,
        verdict: VerdictLike,
        *,
        memory_type: str = "episodic",
    ) -> Dict[str, Any]:
        if _verdict_name(verdict) != "ACCEPT":
            return {"stored": False, "reason": f"verdict={_verdict_name(verdict)}"}
        ts = self.now()
        memory: Dict[str, Any] = {
            "content": str(content),
            "source": str(source),
            "sigma": float(sigma),
            "timestamp": ts.isoformat(),
            "access_count": 0,
            "type": str(memory_type),
            "memory_kind": "episode",
        }
        mem_id = self.layers["episodic"].add(memory)
        memory["id"] = mem_id

        entities = self.extract_entities(str(content))
        relations = self.extract_relations(str(content), entities)
        for entity in entities:
            self.graph.add_node(entity, sigma=float(sigma))
        for subj, rel, obj in relations:
            self.graph.add_edge(subj, obj, relation=rel, sigma=float(sigma))

        self.layers["working"].add(dict(memory))
        return {"stored": True, "id": mem_id, "entities": len(entities)}

    @staticmethod
    def extract_entities(content: str) -> List[str]:
        caps = re.findall(r"\b([A-Z][a-z]+(?:\s+[A-Z][a-z]+)*)\b", content)
        tech = re.findall(r"\b([A-Z]{2,}[a-z0-9]*|[a-z]{3,})\b", content)
        raw = [c.strip() for c in caps + tech if len(c.strip()) > 2]
        seen: set[str] = set()
        out: List[str] = []
        for r in raw:
            k = r.lower()
            if k not in seen:
                seen.add(k)
                out.append(r[:64])
        return out[:24]

    @staticmethod
    def extract_relations(content: str, entities: Sequence[str]) -> List[Tuple[str, str, str]]:
        rels: List[Tuple[str, str, str]] = []
        low = content.lower()
        if " is " in low or " are " in low:
            parts = re.split(r"\bis\b|\bare\b", content, maxsplit=1, flags=re.I)
            if len(parts) == 2:
                subj = parts[0].strip()[:64] or (entities[0] if entities else "subject")
                obj = parts[1].strip().split(".")[0][:120]
                if subj and obj:
                    rels.append((subj[:64], "is", obj[:120]))
        for i in range(len(entities) - 1):
            rels.append((entities[i], "related", entities[i + 1]))
        return rels[:16]

    def vector_search(self, query: str, n: int) -> List[Dict[str, Any]]:
        qt = set(_norm_tokens(query))
        if not qt:
            return []
        scored: List[Tuple[float, Dict[str, Any]]] = []
        for m in self.layers["episodic"].all():
            mt = set(_norm_tokens(str(m.get("content", ""))))
            inter = len(qt & mt)
            union = len(qt | mt) or 1
            score = inter / union
            if score > 0:
                scored.append((score, m))
        scored.sort(key=lambda x: -x[0])
        return [dict(x[1]) for x in scored[:n]]

    def merge_results(self, a: List[Dict[str, Any]], b: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
        by_id: Dict[str, Dict[str, Any]] = {}
        for row in a + b:
            mid = str(row.get("id", ""))
            if not mid:
                continue
            by_id[mid] = dict(row)
        return list(by_id.values())

    def compute_staleness(self, memory: Dict[str, Any]) -> float:
        t0 = _parse_ts(memory)
        age_days = max(0.0, (self.now() - t0).total_seconds() / 86400.0)
        base_decay = 0.001 * age_days
        ac = int(memory.get("access_count", 0) or 0)
        access_factor = 1.0 / (1.0 + ac * 0.1)
        sigma0 = float(memory.get("sigma", 0.0))
        return min(sigma0 + base_decay * access_factor, 1.0)

    def recall(self, query: str, tau: float = 0.3, max_results: int = 10) -> List[Dict[str, Any]]:
        tau_f = float(tau)
        n = max(1, int(max_results))
        vector_results = self.vector_search(str(query), n=max(n * 2, 8))
        entities = self.extract_entities(str(query))
        graph_rows = self.graph.traverse(entities, max_hops=3, tau=0.5)
        graph_hits: List[Dict[str, Any]] = []
        for gr in graph_rows:
            eid = gr.get("entity")
            for m in self.layers["episodic"].all():
                if eid and str(eid).lower() in str(m.get("content", "")).lower():
                    graph_hits.append(dict(m))
        merged = self.merge_results(vector_results, graph_hits)
        reliable: List[Dict[str, Any]] = []
        for r in merged:
            eff = self.compute_staleness(r)
            r2 = dict(r)
            r2["staleness_effective_sigma"] = eff
            r2["effective_sigma"] = eff
            if eff < tau_f:
                reliable.append(r2)
        reliable.sort(key=lambda x: float(x.get("effective_sigma", 1.0)))
        out = reliable[:n]
        for r in out:
            mid = r.get("id")
            if mid:
                self.layers["episodic"].bump_access(str(mid))
        return out

    def distill_to_fact(self, memory: Dict[str, Any]) -> Dict[str, Any]:
        first = str(memory.get("content", "")).strip().split(".")[0][:512]
        ents = self.extract_entities(str(memory.get("content", "")))
        subj = ents[0] if ents else "fact"
        return {
            "content": first,
            "source": "consolidated",
            "subject": subj,
            "entities": ents[:8],
        }

    def consolidate(self) -> Dict[str, Any]:
        frequent = self.layers["episodic"].get_frequent(min_count=3)
        moved = 0
        for bucket in frequent:
            instances: List[Dict[str, Any]] = bucket["instances"]
            template = instances[0]
            fact = self.distill_to_fact(template)
            fact_sigma = min(float(m.get("sigma", 1.0)) for m in instances)
            self.layers["semantic"].add(
                {
                    **fact,
                    "sigma": fact_sigma,
                    "type": "semantic",
                    "memory_kind": "concept",
                    "consolidated_from": len(instances),
                    "timestamp": self.now().isoformat(),
                }
            )
            for inst in instances:
                iid = inst.get("id")
                if not iid:
                    continue
                self.remove(str(iid))
                moved += 1
        return {"consolidated_groups": len(frequent), "episodic_removed": moved}

    def remove(self, mid: str) -> None:
        s = str(mid)
        self.layers["episodic"].remove(s)
        self.layers["working"].remove_by_id(s)

    def forget(
        self,
        *,
        query: Optional[str] = None,
        max_sigma: float = 0.8,
        gdpr: bool = False,
    ) -> Dict[str, Any]:
        if query:
            return {"forgotten": self._forget_targeted(str(query), gdpr=gdpr)}
        n = 0
        for m in list(self.layers["episodic"].all()):
            mid = m.get("id")
            if mid is None:
                continue
            eff = self.compute_staleness(m)
            if eff > float(max_sigma):
                self.remove(str(mid))
                n += 1
        return {"forgotten": n}

    def _forget_targeted(self, query: str, *, gdpr: bool) -> int:
        q = query.lower().strip()
        n = 0
        for m in list(self.layers["episodic"].all()):
            mid = str(m.get("id", ""))
            if not mid:
                continue
            src = str(m.get("source", "")).lower()
            cnt = str(m.get("content", "")).lower()
            blob = f"{cnt} {src}"
            hit = False
            if gdpr:
                uid = q.split(":", 1)[-1].strip() if ":" in q else q
                hit = q in src or q in cnt or (bool(uid) and uid in blob)
            else:
                hit = q in blob
            if hit:
                self.remove(mid)
                n += 1
        return n

    def staleness_report(self) -> List[Dict[str, Any]]:
        rows: List[Dict[str, Any]] = []
        for m in self.layers["episodic"].all() + self.layers["semantic"].all():
            eff = self.compute_staleness(m) if m.get("timestamp") is not None else float(m.get("sigma", 0.0))
            rows.append(
                {
                    "id": m.get("id"),
                    "effective_sigma": eff,
                    "sigma_stored": float(m.get("sigma", 0.0)),
                    "type": m.get("type"),
                }
            )
        rows.sort(key=lambda r: -float(r["effective_sigma"]))
        return rows

    def save_json(self, path: Path) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        payload = {
            "episodic": self.layers["episodic"].all(),
            "semantic": self.layers["semantic"].all(),
            "long_term": self.layers["long_term"].all(),
            "graph_nodes": {k: {"sigma": v.sigma, "type": v.node_type, "created": v.created} for k, v in self.graph.nodes.items()},
            "graph_edges": [
                {"source": e.source, "target": e.target, "relation": e.relation, "sigma": e.sigma, "created": e.created}
                for e in self.graph.edges
            ],
        }
        path.write_text(json.dumps(payload, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    @classmethod
    def load_json(cls, path: Path, **kwargs: Any) -> "EngramV2":
        raw = json.loads(path.read_text(encoding="utf-8"))
        g = cls(**kwargs)
        for row in raw.get("episodic", []):
            g.layers["episodic"].add(row)
        for row in raw.get("semantic", []):
            g.layers["semantic"].add(row)
        for row in raw.get("long_term", []):
            g.layers["long_term"].add(row)
        for name, info in raw.get("graph_nodes", {}).items():
            g.graph.add_node(name, sigma=float(info.get("sigma", 0.0)), node_type=str(info.get("type", "entity")))
        for e in raw.get("graph_edges", []):
            g.graph.add_edge(
                e["source"],
                e["target"],
                str(e.get("relation", "related")),
                sigma=float(e.get("sigma", 0.0)),
            )
        return g


__all__ = [
    "EngramV2",
    "EpisodicMemory",
    "LongTermMemory",
    "SemanticMemory",
    "WorkingMemory",
]
