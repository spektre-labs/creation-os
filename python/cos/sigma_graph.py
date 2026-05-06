# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
σ-graph — knowledge-graph engram: **σ-gate validates every triplet** before storage.

Extends flat engram-style memory with entities and relations. This does **not** replace
`python/cos/engram.py`; it is an optional σ-filtered graph layer. Does not modify ``sigma_gate.h``.

Swap :meth:`extract_triples` for LLM/NER extraction in production; default rules are deterministic.
"""
from __future__ import annotations

import json
import re
from collections import defaultdict
from pathlib import Path
from typing import Any, Callable, DefaultDict, Dict, List, Optional, Sequence, Tuple

from cos.sigma_moe import _gate_score

Triple = Tuple[str, str, str]
TripleExtractor = Callable[[str], Sequence[Triple]]


def _split_sentences(text: str) -> List[str]:
    parts = re.split(r"[.\n;]+", text)
    return [p.strip() for p in parts if p.strip()]


def _default_extract_triples(text: str) -> List[Triple]:
    """Heuristic triple extraction (lab / tests). Replace with LLM pipeline when deployed."""
    out: List[Triple] = []
    for sent in _split_sentences(text):
        m = re.match(r"^(.+?)\s+is\s+the\s+(.+?)\s+of\s+(.+)$", sent, flags=re.IGNORECASE | re.DOTALL)
        if m:
            subj, mid, obj = m.group(1).strip(), m.group(2).strip(), m.group(3).strip()
            rel = re.sub(r"\s+", "_", mid.lower()) + "_of"
            out.append((subj, rel, obj))
            continue
        m = re.match(r"^(.+?)\s+is\s+in\s+(.+)$", sent, flags=re.IGNORECASE | re.DOTALL)
        if m:
            out.append((m.group(1).strip(), "member_of", m.group(2).strip()))
            continue
        m = re.match(r"^(.+?)\s+works\s+for\s+(.+)$", sent, flags=re.IGNORECASE | re.DOTALL)
        if m:
            out.append((m.group(1).strip(), "works_for", m.group(2).strip()))
            continue
    return out


class SigmaGraph:
    """Directed knowledge graph; only **ACCEPT** triplets (per σ-gate) are persisted."""

    def __init__(self, gate: Any, *, extract_triples: Optional[TripleExtractor] = None) -> None:
        self.gate = gate
        self.extract_triples: TripleExtractor = extract_triples or _default_extract_triples
        self.nodes: Dict[str, Dict[str, Any]] = {}
        self.edges: List[Dict[str, Any]] = []
        self.communities: Dict[str, List[str]] = {}
        self._next_entity_id = 0

    def resolve_entity(self, name: str) -> str:
        """Case-folded string match; new names get a fresh ``eN`` id."""
        name_lower = name.lower().strip()
        for eid, node in self.nodes.items():
            if str(node.get("name", "")).lower().strip() == name_lower:
                return str(eid)
        eid = f"e{self._next_entity_id}"
        self._next_entity_id += 1
        return eid

    def add_node(self, entity_id: str, name: str, sigma: float) -> None:
        if entity_id not in self.nodes:
            self.nodes[entity_id] = {
                "name": name,
                "sigma": float(sigma),
                "mentions": 1,
                "edges": 0,
            }
            return
        node = self.nodes[entity_id]
        node["sigma"] = 0.1 * float(sigma) + 0.9 * float(node["sigma"])
        node["mentions"] = int(node.get("mentions", 0)) + 1

    def add_edge(self, source: str, target: str, relation: str, sigma: float, verdict: str) -> None:
        self.edges.append(
            {
                "source": str(source),
                "target": str(target),
                "relation": str(relation),
                "sigma": float(sigma),
                "verdict": str(verdict),
            }
        )
        if source in self.nodes:
            self.nodes[source]["edges"] = int(self.nodes[source].get("edges", 0)) + 1
        if target in self.nodes:
            self.nodes[target]["edges"] = int(self.nodes[target].get("edges", 0)) + 1

    def extract_and_add(self, text: str, source: str = "user") -> Dict[str, Any]:
        triples = list(self.extract_triples(text))
        added: List[Dict[str, Any]] = []
        rejected: List[Dict[str, Any]] = []
        for subj, rel, obj in triples:
            claim = f"{subj} {rel} {obj}"
            sigma, verdict = _gate_score(self.gate, text, claim)
            if verdict != "ACCEPT":
                rejected.append({"triple": (subj, rel, obj), "sigma": sigma, "verdict": verdict})
                continue
            subj_id = self.resolve_entity(subj)
            obj_id = self.resolve_entity(obj)
            self.add_node(subj_id, subj, sigma)
            self.add_node(obj_id, obj, sigma)
            self.add_edge(subj_id, obj_id, rel, sigma, verdict)
            added.append({"triple": (subj, rel, obj), "sigma": sigma, "source": source})
        return {
            "added": len(added),
            "rejected": len(rejected),
            "details": added,
            "rejected_details": rejected,
        }

    def match_entities(self, question: str) -> List[str]:
        ql = question.lower()
        matched: List[str] = []
        for eid, node in self.nodes.items():
            name = str(node.get("name", "")).strip()
            if name and name.lower() in ql:
                matched.append(str(eid))
        return matched

    def query(self, question: str, max_hops: int = 2, tau: float = 0.3) -> Dict[str, Any]:
        """BFS multi-hop retrieval; skip edges with σ above ``tau`` (high σ = less trusted)."""
        query_entities = self.match_entities(question)
        if not query_entities:
            return {"results": [], "reason": "no matching entities", "entities_traversed": 0}

        visited: set[str] = set()
        results: List[Dict[str, Any]] = []
        seen_edges: set[Tuple[str, str, str]] = set()
        frontier: List[Tuple[str, int]] = [(e, 0) for e in query_entities]
        max_hops_used = 0

        while frontier:
            entity_id, hop = frontier.pop(0)
            if entity_id in visited or hop > max_hops:
                continue
            visited.add(entity_id)
            max_hops_used = max(max_hops_used, hop)
            for edge in self.edges:
                if float(edge["sigma"]) > float(tau):
                    continue
                src, tgt = edge["source"], edge["target"]
                ek: Tuple[str, str, str] = (str(src), str(tgt), str(edge["relation"]))
                if src == entity_id:
                    if ek not in seen_edges:
                        seen_edges.add(ek)
                        results.append(edge)
                    frontier.append((str(tgt), hop + 1))
                elif tgt == entity_id:
                    if ek not in seen_edges:
                        seen_edges.add(ek)
                        results.append(edge)
                    frontier.append((str(src), hop + 1))

        results.sort(key=lambda x: float(x["sigma"]))
        return {
            "results": results,
            "entities_traversed": len(visited),
            "hops_used": max_hops_used,
            "sigma_filter": float(tau),
        }

    def detect_conflicts(self) -> List[Dict[str, Any]]:
        """Same entity pair (undirected), multiple distinct relations."""
        edge_map: DefaultDict[Tuple[str, str], List[Dict[str, Any]]] = defaultdict(list)
        for edge in self.edges:
            a, b = str(edge["source"]), str(edge["target"])
            key = tuple(sorted((a, b)))
            edge_map[key].append(edge)
        conflicts: List[Dict[str, Any]] = []
        for key, elist in edge_map.items():
            if len(elist) <= 1:
                continue
            relations = {str(e["relation"]) for e in elist}
            if len(relations) > 1:
                conflicts.append(
                    {
                        "entities": key,
                        "conflicting_relations": sorted(relations),
                        "sigmas": [float(e["sigma"]) for e in elist],
                    }
                )
        return conflicts

    def _recompute_edge_counts(self) -> None:
        for n in self.nodes.values():
            n["edges"] = 0
        for e in self.edges:
            s, t = e["source"], e["target"]
            if s in self.nodes:
                self.nodes[s]["edges"] = int(self.nodes[s]["edges"]) + 1
            if t in self.nodes:
                self.nodes[t]["edges"] = int(self.nodes[t]["edges"]) + 1

    def forget(self, max_sigma: float = 0.8) -> Dict[str, int]:
        """Drop high-σ edges (noise / “unlearn” sketch; not legal advice)."""
        before = len(self.edges)
        self.edges = [e for e in self.edges if float(e["sigma"]) <= float(max_sigma)]
        self._recompute_edge_counts()
        after = len(self.edges)
        return {"removed": before - after, "remaining": after}

    def stats(self) -> Dict[str, Any]:
        nn = len(self.nodes)
        ne = len(self.edges)
        avg_n = sum(float(n["sigma"]) for n in self.nodes.values()) / max(nn, 1)
        avg_e = sum(float(e["sigma"]) for e in self.edges) / max(ne, 1)
        return {
            "nodes": nn,
            "edges": ne,
            "avg_node_sigma": avg_n,
            "avg_edge_sigma": avg_e,
            "conflicts": len(self.detect_conflicts()),
        }

    def to_blob(self) -> Dict[str, Any]:
        return {
            "_next_entity_id": self._next_entity_id,
            "nodes": dict(self.nodes),
            "edges": list(self.edges),
            "communities": dict(self.communities),
        }

    def load_blob(self, blob: Dict[str, Any]) -> None:
        self._next_entity_id = int(blob.get("_next_entity_id", 0))
        nodes = blob.get("nodes")
        self.nodes = dict(nodes) if isinstance(nodes, dict) else {}
        edges = blob.get("edges")
        self.edges = list(edges) if isinstance(edges, list) else []
        comm = blob.get("communities")
        self.communities = dict(comm) if isinstance(comm, dict) else {}


def default_sigma_graph_path() -> Path:
    d = Path.home() / ".cos"
    d.mkdir(parents=True, exist_ok=True)
    return d / "sigma_graph_state.json"


def load_graph_from_path(path: Path, gate: Any, *, extract_triples: Optional[TripleExtractor] = None) -> SigmaGraph:
    g = SigmaGraph(gate, extract_triples=extract_triples)
    blob: dict[str, Any] | None = None
    if path.is_file():
        try:
            raw = json.loads(path.read_text(encoding="utf-8"))
            blob = raw if isinstance(raw, dict) else None
        except (OSError, json.JSONDecodeError):
            blob = None
    if blob is not None:
        g.load_blob(blob)
    return g


def save_graph_to_path(path: Path, g: SigmaGraph) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(g.to_blob(), indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


__all__ = [
    "SigmaGraph",
    "default_sigma_graph_path",
    "load_graph_from_path",
    "save_graph_to_path",
    "_default_extract_triples",
]
