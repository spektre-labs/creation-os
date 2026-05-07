# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from cos.causal import CausalEdge, CausalGraph
from cos.graph import SigmaGraph


def test_add_causal_edge_with_sigma() -> None:
    cg = CausalGraph()
    out = cg.add("Stress", "Burnout", sigma=0.25)
    assert out.get("added") is True
    assert out["sigma"] == 0.25
    assert len(cg.edges) == 1
    assert isinstance(cg.edges[0], CausalEdge)


def test_causes_of_returns_parents() -> None:
    cg = CausalGraph()
    cg.add("rain", "wet", sigma=0.1)
    parents = cg.causes_of("wet")
    assert len(parents) == 1
    assert parents[0]["cause"] == "rain"


def test_effects_of_returns_children() -> None:
    cg = CausalGraph()
    cg.add("rain", "wet", sigma=0.1)
    children = cg.effects_of("rain")
    assert len(children) == 1
    assert children[0]["effect"] == "wet"


def test_do_severs_incoming_edges() -> None:
    cg = CausalGraph()
    cg.add("a", "b", sigma=0.1)
    cg.add("c", "b", sigma=0.2)
    cg.add("b", "d", sigma=0.15)
    out = cg.do("b")
    assert set(out["severed_causes"]) == {"a", "c"}


def test_do_keeps_outgoing_effects() -> None:
    cg = CausalGraph()
    cg.add("a", "b", sigma=0.1)
    cg.add("c", "b", sigma=0.2)
    cg.add("b", "d", sigma=0.15)
    out = cg.do("b")
    assert "d" in out["downstream_effects"]
    assert "σ_total" in out


def test_root_cause_finds_deepest() -> None:
    cg = CausalGraph()
    cg.add("a", "b", sigma=0.1)
    cg.add("b", "c", sigma=0.12)
    roots = cg.root_cause("c")
    assert any(r["root"] == "a" for r in roots)
    assert any(len(r["path"]) >= 3 for r in roots)


def test_root_cause_sorts_by_sigma() -> None:
    cg = CausalGraph()
    cg.add("lo", "z", sigma=0.1)
    cg.add("hi", "z", sigma=0.9)
    roots = cg.root_cause("z")
    assert len(roots) == 2
    assert roots[0]["cumulative_σ"] <= roots[1]["cumulative_σ"]


def test_counterfactual_returns_result() -> None:
    cg = CausalGraph()
    cg.add("smoke", "fire", sigma=0.2)
    obs = {"smoke": 1, "fire": 1}
    intervention = {"fire": 0}
    out = cg.counterfactual(obs, intervention, "fire")
    assert out["counterfactual_outcome"] == 0
    assert "σ" in out
    assert out["path"] == ["smoke", "fire"]


def test_find_path_exists() -> None:
    cg = CausalGraph()
    cg.add("a", "b", sigma=0.1)
    cg.add("b", "c", sigma=0.11)
    p = cg.find_path("a", "c")
    assert p == ["a", "b", "c"]


def test_find_path_none() -> None:
    cg = CausalGraph()
    cg.add("a", "b", sigma=0.1)
    assert cg.find_path("a", "x") is None
    assert CausalGraph().add("x", "x", sigma=0.1).get("added") is False


def test_sigma_graph_to_causal() -> None:
    sg = SigmaGraph(write_threshold=1.0)
    sg.add("heat", "causes", "expansion", sigma=0.2)
    cg = sg.to_causal_graph()
    assert any(e.cause == "heat" and e.effect == "expansion" for e in cg.edges)
