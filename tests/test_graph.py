# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from cos.graph import SigmaGraph
from cos.jepa import SigmaJEPA


def test_embed_triple_in_jepa_latent() -> None:
    graph = SigmaGraph()
    j = SigmaJEPA(dim=32)
    v = graph.embed_triple("Paris", "is_capital_of", "France", j)
    assert len(v) == 32 or (hasattr(v, "shape") and int(v.shape[0]) == 32)  # type: ignore[attr-defined]


def test_add_triple() -> None:
    graph = SigmaGraph()
    result = graph.add("Paris", "is_capital_of", "France", sigma=0.05)
    assert result["added"]
    assert graph.stats()["triples"] == 1


def test_reject_high_sigma() -> None:
    graph = SigmaGraph(write_threshold=0.3)
    result = graph.add("fake", "relation", "entity", sigma=0.9)
    assert not result["added"]
    assert result["reason"] == "sigma_too_high"


def test_query() -> None:
    graph = SigmaGraph()
    graph.add("Paris", "is_capital_of", "France", sigma=0.05)
    graph.add("Paris", "is_in", "Europe", sigma=0.08)
    results = graph.query("Paris")
    assert len(results) == 2


def test_query_by_relation() -> None:
    graph = SigmaGraph()
    graph.add("Paris", "is_capital_of", "France", sigma=0.05)
    graph.add("Paris", "is_in", "Europe", sigma=0.08)
    results = graph.query("Paris", relation="is_capital_of")
    assert len(results) == 1


def test_multi_hop() -> None:
    graph = SigmaGraph()
    graph.add("Marie Curie", "discovered", "radium", sigma=0.05)
    graph.add("radium", "used_for", "cancer treatment", sigma=0.08)
    path = graph.multi_hop("Marie Curie", "cancer treatment")
    assert path["found"]
    assert path["hops"] == 2


def test_conflict_resolution() -> None:
    graph = SigmaGraph()
    graph.add("France", "capital", "Paris", sigma=0.05)
    result = graph.add("France", "capital", "London", sigma=0.9)
    assert not result["added"]


def test_conflict_lower_sigma_wins() -> None:
    # Lyon at σ=0.6 needs headroom above default 0.5 write threshold
    graph = SigmaGraph(write_threshold=0.7)
    graph.add("France", "capital", "Lyon", sigma=0.6)
    result = graph.add("France", "capital", "Paris", sigma=0.05)
    assert result["added"]
    results = graph.query("France", relation="capital")
    assert len(results) == 1
    assert results[0].object == "Paris"


def test_subgraph() -> None:
    graph = SigmaGraph()
    graph.add("A", "rel", "B", sigma=0.1)
    graph.add("B", "rel", "C", sigma=0.1)
    graph.add("C", "rel", "D", sigma=0.1)
    sub = graph.subgraph("A", depth=2)
    assert "a" in sub["nodes"]
    assert "b" in sub["nodes"]
    assert "c" in sub["nodes"]


def test_stats() -> None:
    graph = SigmaGraph()
    graph.add("X", "r", "Y", sigma=0.1)
    graph.add("Y", "r", "Z", sigma=0.2)
    stats = graph.stats()
    assert stats["triples"] == 2
    assert stats["entities"] == 3
    assert stats["relations"] == 1


def test_extract() -> None:
    graph = SigmaGraph()
    result = graph.extract("Paris is the capital of France")
    assert result["extracted"] >= 1
