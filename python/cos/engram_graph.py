# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
Engram **Knowledge Graph** (lab): σ per node and edge, multi-hop traversal with τ cut-offs.

``backend="sqlite"`` reserves a future persistence path; the default is in-memory only.
See ``docs/CLAIM_DISCIPLINE.md`` — no Mem0 / GAAMA reproduction claims.
"""
from __future__ import annotations

import re
import time
from dataclasses import dataclass
from typing import Any, Callable, Dict, List, Optional, Set, Tuple


def _now_mono() -> float:
    return time.monotonic()


@dataclass
class _Node:
    entity: str
    sigma: float
    node_type: str
    created: float


@dataclass
class _Edge:
    source: str
    target: str
    relation: str
    sigma: float
    created: float


class KnowledgeGraph:
    """Directed multi-graph with optional undirected traversal for recall."""

    def __init__(self, backend: str = "sqlite", *, clock: Optional[Callable[[], float]] = None) -> None:
        self.backend = str(backend)
        self._clock = clock or time.time
        self.nodes: Dict[str, _Node] = {}
        self.edges: List[_Edge] = []

    def now(self) -> float:
        return float(self._clock())

    def add_node(self, entity: str, sigma: float = 0.0, node_type: str = "entity") -> None:
        e = str(entity).strip()
        if not e:
            return
        self.nodes[e] = _Node(entity=e, sigma=float(sigma), node_type=str(node_type), created=self.now())

    def add_edge(self, source: str, target: str, relation: str, sigma: float = 0.0) -> None:
        s, t = str(source).strip(), str(target).strip()
        if not s or not t:
            return
        if s not in self.nodes:
            self.add_node(s, sigma=sigma)
        if t not in self.nodes:
            self.add_node(t, sigma=sigma)
        self.edges.append(_Edge(source=s, target=t, relation=str(relation), sigma=float(sigma), created=self.now()))

    def traverse(
        self,
        start_entities: List[str],
        *,
        max_hops: int = 3,
        tau: float = 0.5,
    ) -> List[Dict[str, Any]]:
        """BFS; skip nodes/edges with σ ≥ τ (uncertainty too high for this lab convention)."""
        t = float(tau)
        visited: Set[str] = set()
        results: List[Dict[str, Any]] = []
        frontier: List[Tuple[str, int]] = [(str(e).strip(), 0) for e in start_entities if str(e).strip() in self.nodes]

        while frontier:
            entity, depth = frontier.pop(0)
            if entity in visited or depth > int(max_hops):
                continue
            visited.add(entity)
            node = self.nodes.get(entity)
            if node is None or node.sigma >= t:
                continue
            results.append(
                {
                    "entity": entity,
                    "depth": depth,
                    "type": node.node_type,
                    "sigma": node.sigma,
                    "created": node.created,
                }
            )
            for edge in self.edges:
                if edge.sigma >= t:
                    continue
                if edge.source == entity:
                    frontier.append((edge.target, depth + 1))
                elif edge.target == entity:
                    frontier.append((edge.source, depth + 1))
        return results

    def find_related(self, new_fact: Dict[str, Any]) -> List[Dict[str, Any]]:
        """Nodes sharing entities or incident edges (lab stub)."""
        ents = list(new_fact.get("entities") or [])
        out: List[Dict[str, Any]] = []
        for e in ents:
            if e in self.nodes:
                out.append({"entity": e, "node": self.nodes[e]})
        for edge in self.edges:
            if edge.source in ents or edge.target in ents:
                out.append({"edge": edge})
        return out

    def contradicts(self, new_fact: Dict[str, Any], existing: Dict[str, Any]) -> bool:
        """Cheap contradiction check: opposing polarity tokens on same subject token."""
        subj = str(new_fact.get("subject", "") or "").lower()
        if not subj:
            return False
        pol_new = _polarity(str(new_fact.get("polarity", new_fact.get("content", ""))))
        if "node" in existing:
            return False
        if "edge" in existing:
            edge = existing["edge"]
            if subj not in (edge.source.lower(), edge.target.lower()):
                return False
            pol_old = _polarity(edge.relation)
            return pol_new * pol_old < 0
        return False

    def conflict_detection(self, new_fact: Dict[str, Any]) -> List[Dict[str, Any]]:
        existing = self.find_related(new_fact)
        conflicts: List[Dict[str, Any]] = []
        for e in existing:
            if self.contradicts(new_fact, e):
                conflicts.append({"existing": _serialize_related(e), "new": new_fact, "resolution": "keep_lower_sigma"})
        return conflicts


def _serialize_related(x: Dict[str, Any]) -> Dict[str, Any]:
    if "node" in x:
        n: _Node = x["node"]
        return {"entity": n.entity, "sigma": n.sigma, "type": n.node_type}
    if "edge" in x:
        e: _Edge = x["edge"]
        return {"source": e.source, "target": e.target, "relation": e.relation, "sigma": e.sigma}
    return dict(x)


def _polarity(text: str) -> int:
    t = text.lower()
    neg = bool(re.search(r"\b(not|no|never|false|isn't|aren't)\b", t))
    pos = bool(re.search(r"\b(is|are|true|yes|always)\b", t))
    if neg and not pos:
        return -1
    if pos and not neg:
        return 1
    return 0


__all__ = ["KnowledgeGraph"]
