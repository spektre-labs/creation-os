# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""v149 σ-graph: σ-gated triplets, multi-hop query, conflicts, forget."""
from __future__ import annotations

from pathlib import Path

from cos.sigma_graph import SigmaGraph, load_graph_from_path, save_graph_to_path


class _MockAccept:
    def score(self, prompt: str, response: str):
        return 0.05, "ACCEPT"


class _MockTriageTwin:
    def score(self, prompt: str, response: str):
        if "twin_city" in response or "twin city" in response.lower():
            return 0.9, "ABSTAIN"
        return 0.04, "ACCEPT"


def test_three_triplets_two_accept_one_reject() -> None:
    text = (
        "Helsinki is the capital of Finland. "
        "Finland is in the EU. "
        "Helsinki is the twin city of Nowhere."
    )
    g = SigmaGraph(_MockTriageTwin())
    r = g.extract_and_add(text)
    assert r["added"] == 2
    assert r["rejected"] == 1
    assert len(g.edges) == 2
    assert len(g.nodes) >= 3


def test_multi_hop_query_chain() -> None:
    text = "Helsinki is the capital of Finland. Finland is in the EU."
    g = SigmaGraph(_MockAccept())
    g.extract_and_add(text)
    out = g.query("What organizations is Helsinki part of?", max_hops=2, tau=0.9)
    rels = {str(e["relation"]) for e in out["results"]}
    assert "capital_of" in rels
    assert "member_of" in rels
    assert out["entities_traversed"] >= 1


def test_conflicting_relations_detected() -> None:
    g = SigmaGraph(_MockAccept())
    g.nodes = {
        "e0": {"name": "Acme", "sigma": 0.1, "mentions": 1, "edges": 0},
        "e1": {"name": "Beta", "sigma": 0.1, "mentions": 1, "edges": 0},
    }
    g._next_entity_id = 2
    g.edges = []
    g.add_edge("e0", "e1", "supplier_of", 0.12, "ACCEPT")
    g.add_edge("e0", "e1", "customer_of", 0.15, "ACCEPT")
    c = g.detect_conflicts()
    assert len(c) == 1
    assert set(c[0]["conflicting_relations"]) == {"customer_of", "supplier_of"}


def test_forget_high_sigma_edges() -> None:
    g = SigmaGraph(_MockAccept())
    g.nodes = {
        "a": {"name": "A", "sigma": 0.1, "mentions": 1, "edges": 0},
        "b": {"name": "B", "sigma": 0.1, "mentions": 1, "edges": 0},
        "c": {"name": "C", "sigma": 0.1, "mentions": 1, "edges": 0},
    }
    g._next_entity_id = 3
    g.edges = [
        {"source": "a", "target": "b", "relation": "r", "sigma": 0.6, "verdict": "ACCEPT"},
        {"source": "a", "target": "c", "relation": "r2", "sigma": 0.2, "verdict": "ACCEPT"},
    ]
    g._recompute_edge_counts()
    r = g.forget(max_sigma=0.5)
    assert r["removed"] == 1
    assert r["remaining"] == 1
    assert len(g.edges) == 1
    assert float(g.edges[0]["sigma"]) <= 0.5


def test_persist_roundtrip(tmp_path: Path) -> None:
    path = tmp_path / "sg.json"
    g1 = SigmaGraph(_MockAccept())
    g1.extract_and_add("Helsinki is the capital of Finland.")
    save_graph_to_path(path, g1)
    g2 = load_graph_from_path(path, _MockAccept())
    assert len(g2.nodes) == len(g1.nodes)
    assert len(g2.edges) == 1
