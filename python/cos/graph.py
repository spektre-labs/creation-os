# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-graph — knowledge-graph lab store with σ-gated writes per triple.

Each edge is ``(subject, relation, object)`` scored by ``SigmaGate`` before storage; high
σ is rejected or loses to a lower-σ rival on the same ``(subject, relation)`` key.
This is a **portable lab** graph (naive extraction, BFS paths), not a GraphRAG benchmark;
see ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import hashlib
import time
from typing import Any, Dict, List, Optional, Set, Tuple

__all__ = ["SigmaGraph", "Triple"]


class Triple:
    """One triple: (subject, relation, object) with σ and optional provenance."""

    __slots__ = ("subject", "relation", "object", "sigma", "source", "timestamp", "id")

    def __init__(
        self,
        subject: str,
        relation: str,
        obj: str,
        sigma: float = 0.0,
        source: Optional[str] = None,
        *,
        timestamp: Optional[float] = None,
    ) -> None:
        self.subject = str(subject)
        self.relation = str(relation)
        self.object = str(obj)
        self.sigma = float(sigma)
        self.source = source
        self.timestamp = float(time.time() if timestamp is None else timestamp)
        payload = f"{self.subject}:{self.relation}:{self.object}".encode("utf-8")
        self.id = hashlib.sha256(payload).hexdigest()[:12]

    def to_dict(self) -> Dict[str, Any]:
        return {
            "id": self.id,
            "subject": self.subject,
            "relation": self.relation,
            "object": self.object,
            "sigma": self.sigma,
            "created": self.timestamp,
            "source": self.source,
        }

    def __repr__(self) -> str:
        return f"({self.subject} —[{self.relation}]→ {self.object}, σ={self.sigma:.3f})"

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, Triple):
            return NotImplemented
        return (
            self.subject == other.subject
            and self.relation == other.relation
            and self.object == other.object
        )

    def __hash__(self) -> int:
        return hash((self.subject, self.relation, self.object))


class SigmaGraph:
    """σ-validated triple store with query, multi-hop BFS, and bounded subgraphs."""

    def __init__(self, gate: Any = None, *, write_threshold: float = 0.5) -> None:
        from cos.sigma_gate import SigmaGate

        self.gate = gate or SigmaGate()
        self.write_threshold = float(write_threshold)
        self.triples: Dict[str, Triple] = {}
        self.index_subject: Dict[str, Set[str]] = {}
        self.index_object: Dict[str, Set[str]] = {}
        self.index_relation: Dict[str, Set[str]] = {}
        self.rejected: List[Triple] = []

    def add(
        self,
        subject: str,
        relation: str,
        obj: str,
        sigma: Optional[float] = None,
        source: Optional[str] = None,
    ) -> Dict[str, Any]:
        if sigma is None:
            statement = f"{subject} {relation} {obj}"
            sigma, _ = self.gate.score("knowledge", statement)
        sigma = float(sigma)

        triple = Triple(subject, relation, obj, sigma, source)

        if sigma > self.write_threshold:
            self.rejected.append(triple)
            return {"added": False, "sigma": sigma, "reason": "sigma_too_high"}

        conflicts = self._find_conflicts(triple)
        if conflicts:
            for conflict in conflicts:
                if not (triple.sigma < conflict.sigma):
                    self.rejected.append(triple)
                    return {
                        "added": False,
                        "sigma": sigma,
                        "reason": "conflict_higher_sigma",
                        "conflicting": str(conflict),
                    }
            for conflict in conflicts:
                self._remove(conflict.id)

        self.triples[triple.id] = triple
        self._index_add(triple)

        return {"added": True, "id": triple.id, "sigma": sigma}

    def embed_triple(self, subject: str, relation: str, obj: str, jepa: Any) -> Any:
        """Encode ``(subject, relation, object)`` into the same latent space as ``jepa.encode``."""
        statement = f"{subject} {relation} {obj}"
        return jepa.encode(statement)

    def extract(self, text: str, source: Optional[str] = None) -> Dict[str, Any]:
        triples_found = self._simple_extract(text)
        results: List[Dict[str, Any]] = []
        prov = source or text[:50]
        for subj, rel, o in triples_found:
            results.append(self.add(subj, rel, o, source=prov))
        return {
            "extracted": len(triples_found),
            "added": sum(1 for r in results if r.get("added")),
            "rejected": sum(1 for r in results if not r.get("added")),
            "results": results,
        }

    def query(self, entity: str, relation: Optional[str] = None) -> List[Triple]:
        ent = entity.lower()
        results: List[Triple] = []

        for tid in self.index_subject.get(ent, set()):
            triple = self.triples.get(tid)
            if triple and (relation is None or triple.relation == relation):
                results.append(triple)

        seen: Set[str] = {t.id for t in results}
        for tid in self.index_object.get(ent, set()):
            triple = self.triples.get(tid)
            if triple and triple.id not in seen:
                if relation is None or triple.relation == relation:
                    results.append(triple)
                    seen.add(triple.id)

        results.sort(key=lambda t: t.sigma)
        return results

    def multi_hop(
        self,
        start: str,
        target: str,
        *,
        max_hops: int = 3,
    ) -> Dict[str, Any]:
        target_lower = target.lower()
        queue: List[Tuple[str, List[Triple], float]] = [(start.lower(), [], 0.0)]
        visited: Set[str] = set()

        while queue:
            current, path, cum_sigma = queue.pop(0)

            if current == target_lower:
                n = max(len(path), 1)
                return {
                    "found": True,
                    "path": path,
                    "hops": len(path),
                    "cumulative_sigma": round(cum_sigma, 4),
                    "reliable": (cum_sigma / n) < 0.3,
                }

            if current in visited or len(path) >= max_hops:
                continue

            visited.add(current)

            for tid in self.index_subject.get(current, set()):
                triple = self.triples.get(tid)
                if triple:
                    nxt = triple.object.lower()
                    if nxt not in visited:
                        queue.append((nxt, path + [triple], cum_sigma + triple.sigma))

            for tid in self.index_object.get(current, set()):
                triple = self.triples.get(tid)
                if triple:
                    nxt = triple.subject.lower()
                    if nxt not in visited:
                        queue.append((nxt, path + [triple], cum_sigma + triple.sigma))

        return {"found": False, "hops": max_hops, "reason": "no path found"}

    def subgraph(self, entity: str, depth: int = 2) -> Dict[str, Any]:
        center = entity.lower()
        nodes: Set[str] = set()
        edge_objs: List[Triple] = []

        queue: List[Tuple[str, int]] = [(center, 0)]
        visited: Set[str] = set()

        while queue:
            current, d = queue.pop(0)
            if d > depth or current in visited:
                continue
            visited.add(current)
            nodes.add(current)

            for tid in self.index_subject.get(current, set()):
                triple = self.triples.get(tid)
                if triple:
                    edge_objs.append(triple)
                    nxt = triple.object.lower()
                    nodes.add(nxt)
                    queue.append((nxt, d + 1))

            for tid in self.index_object.get(current, set()):
                triple = self.triples.get(tid)
                if triple:
                    edge_objs.append(triple)
                    nxt = triple.subject.lower()
                    nodes.add(nxt)
                    queue.append((nxt, d + 1))

        return {
            "center": entity,
            "depth": depth,
            "nodes": sorted(nodes),
            "edges": [str(e) for e in edge_objs],
            "n_nodes": len(nodes),
            "n_edges": len(edge_objs),
        }

    def stats(self) -> Dict[str, Any]:
        vals = list(self.triples.values())
        n = len(vals)
        entities = set()
        for t in vals:
            entities.add(t.subject.lower())
            entities.add(t.object.lower())
        rels = {t.relation for t in vals}
        avg = sum(t.sigma for t in vals) / max(n, 1)
        return {
            "triples": n,
            "entities": len(entities),
            "relations": len(rels),
            "rejected": len(self.rejected),
            "avg_sigma": avg,
        }

    def discard(self, triple_id: str) -> bool:
        """Remove a triple by id (maintenance / dream lab). Returns False if missing."""
        tid = str(triple_id).strip()
        if tid not in self.triples:
            return False
        self._remove(tid)
        return True

    def _insert_direct(self, triple: Triple) -> bool:
        """Insert a pre-built triple without re-scoring (internal maintenance path)."""
        if triple.sigma > self.write_threshold:
            self.rejected.append(triple)
            return False
        conflicts = self._find_conflicts(triple)
        if conflicts:
            for conflict in conflicts:
                if not (triple.sigma < conflict.sigma):
                    self.rejected.append(triple)
                    return False
            for conflict in conflicts:
                self._remove(conflict.id)
        self.triples[triple.id] = triple
        self._index_add(triple)
        return True

    def entities(self) -> List[str]:
        """Distinct entity labels (first-seen casing) appearing in any triple."""
        canon: Dict[str, str] = {}
        for t in self.triples.values():
            for lbl in (t.subject, t.object):
                k = lbl.lower()
                if k not in canon:
                    canon[k] = lbl
        return sorted(canon.values(), key=lambda x: x.lower())

    def all_triples(self) -> List[Dict[str, Any]]:
        """All edges as JSON-serializable dicts (``created`` = UNIX timestamp)."""
        return [t.to_dict() for t in self.triples.values()]

    def neighbors(self, entity: str) -> List[str]:
        """Adjacent entities (undirected, excludes self-loops as neighbors)."""
        el = entity.lower()
        out: Set[str] = set()
        for t in self.triples.values():
            if t.subject.lower() == el and t.object.lower() != el:
                out.add(t.object)
            elif t.object.lower() == el and t.subject.lower() != el:
                out.add(t.subject)
        return sorted(out, key=lambda x: x.lower())

    def has_edge(self, a: str, b: str) -> bool:
        """True if some triple connects ``a`` and ``b`` (either direction)."""
        al, bl = a.lower(), b.lower()
        for t in self.triples.values():
            s, o = t.subject.lower(), t.object.lower()
            if (s == al and o == bl) or (s == bl and o == al):
                return True
        return False

    def relations_of(self, entity: str) -> List[Dict[str, Any]]:
        """Incident triples as dicts (subject, relation, object, sigma)."""
        el = entity.lower()
        rows: List[Dict[str, Any]] = []
        for t in self.triples.values():
            if t.subject.lower() == el or t.object.lower() == el:
                rows.append(
                    {
                        "id": t.id,
                        "subject": t.subject,
                        "relation": t.relation,
                        "object": t.object,
                        "sigma": float(t.sigma),
                    }
                )
        return rows

    def avg_sigma(self, entity: str) -> float:
        """Mean σ over triples incident to ``entity`` (0 if isolated)."""
        rels = self.relations_of(entity)
        if not rels:
            return 0.0
        return float(sum(r["sigma"] for r in rels)) / len(rels)

    def update_sigma(self, triple_id: str, new_sigma: float) -> bool:
        """Adjust stored σ; drops the triple if it crosses ``write_threshold``."""
        tid = str(triple_id).strip()
        t = self.triples.get(tid)
        if t is None:
            return False
        ns = min(1.0, max(0.0, float(new_sigma)))
        if ns > self.write_threshold:
            self._remove(tid)
            self.rejected.append(
                Triple(t.subject, t.relation, t.object, ns, t.source, timestamp=t.timestamp)
            )
            return True
        t.sigma = ns
        return True

    def remove_entity(self, entity: str) -> int:
        """Remove all triples touching ``entity`` (case-insensitive). Returns count removed."""
        el = entity.lower()
        removed = 0
        for tid in list(self.triples.keys()):
            t = self.triples.get(tid)
            if t and (t.subject.lower() == el or t.object.lower() == el):
                self._remove(tid)
                removed += 1
        return removed

    def merge_entities(self, canonical: str, absorbed: str) -> None:
        """Rename every occurrence of ``absorbed`` into ``canonical`` and de-duplicate edges."""
        a, b = str(canonical).strip(), str(absorbed).strip()
        if not a or not b or a.lower() == b.lower():
            return
        buf: List[Triple] = []
        for t in list(self.triples.values()):
            self._remove(t.id)
            ns = a if t.subject.lower() == b.lower() else t.subject
            no = a if t.object.lower() == b.lower() else t.object
            buf.append(Triple(ns, t.relation, no, t.sigma, t.source, timestamp=t.timestamp))
        best: Dict[Tuple[str, str, str], Triple] = {}
        for t in buf:
            key = (t.subject.lower(), t.relation.lower(), t.object.lower())
            if key not in best or t.sigma < best[key].sigma:
                best[key] = t
        for t in best.values():
            self._insert_direct(t)

    def _find_conflicts(self, triple: Triple) -> List[Triple]:
        conflicts: List[Triple] = []
        for tid in self.index_subject.get(triple.subject.lower(), set()):
            existing = self.triples.get(tid)
            if (
                existing
                and existing.relation == triple.relation
                and existing.object != triple.object
            ):
                conflicts.append(existing)
        return conflicts

    def _index_add(self, triple: Triple) -> None:
        subj = triple.subject.lower()
        obj = triple.object.lower()
        rel = triple.relation.lower()
        self.index_subject.setdefault(subj, set()).add(triple.id)
        self.index_object.setdefault(obj, set()).add(triple.id)
        self.index_relation.setdefault(rel, set()).add(triple.id)

    def _remove(self, triple_id: str) -> None:
        triple = self.triples.pop(triple_id, None)
        if triple:
            self.index_subject.get(triple.subject.lower(), set()).discard(triple_id)
            self.index_object.get(triple.object.lower(), set()).discard(triple_id)
            self.index_relation.get(triple.relation.lower(), set()).discard(triple_id)

    def _simple_extract(self, text: str) -> List[Tuple[str, str, str]]:
        out: List[Tuple[str, str, str]] = []
        for raw in text.replace(".", ".\n").split("\n"):
            sent = raw.strip()
            if not sent:
                continue
            words = sent.split()
            if len(words) >= 3:
                subj = words[0]
                rel = words[1]
                obj = " ".join(words[2:]).rstrip(".")
                if subj and rel and obj:
                    out.append((subj, rel, obj))
        return out
