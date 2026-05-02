# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Integration tests for σ-swarm fleet (mesh pheromone lab)."""
from __future__ import annotations

import json
import sys
import tempfile
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.sigma_swarm import SigmaSwarmFleet, SwarmNodeGate, _query_hash  # noqa: E402


def test_fleet_consensus_selects_lowest_sigma_node() -> None:
    fl = SigmaSwarmFleet()
    fl.add_node("low", bias=-0.18)
    fl.add_node("mid", bias=0.0)
    fl.add_node("high", bias=0.18)
    prompt = "Explain quantum computing"
    out = fl.swarm_query(prompt, "consensus")
    assert out["strategy"] == "consensus"
    assert out["selected_node"] == "low"
    assert out["total_nodes"] == 3
    assert float(out["sigma"]) == min(float(s) for s in out["all_sigmas"].values())


def test_fleet_pheromone_merge_after_neighbor_queries() -> None:
    fl = SigmaSwarmFleet()
    fl.add_node("a", bias=0.0)
    fl.add_node("b", bias=0.0)
    p = "shared trace prompt"
    qh = _query_hash(p)
    fl.nodes["a"].query(p)
    fl.nodes["b"].query(p)
    assert fl.nodes["a"].pheromone_trail[qh].get("consensus") is True


def test_fleet_specialist_prefers_low_avg_sigma_history() -> None:
    fl = SigmaSwarmFleet()
    fl.add_node("good", bias=-0.08)
    fl.add_node("bad", bias=0.32)
    for _ in range(10):
        fl.nodes["bad"].query("bad domain habit")
    for _ in range(10):
        fl.nodes["good"].query("good domain habit")
    assert fl.nodes["bad"].gate.avg_sigma() > fl.nodes["good"].gate.avg_sigma()
    out = fl.swarm_query("fresh specialist prompt", "specialist")
    assert out["strategy"] == "specialist"
    assert out["node"] == "good"


def test_fleet_remove_node_adapts_mesh() -> None:
    fl = SigmaSwarmFleet()
    fl.add_node("railo", bias=0.0)
    fl.add_node("hanna", bias=0.0)
    fl.add_node("niko", bias=0.0)
    assert len(fl.nodes["railo"].peers) == 2
    fl.remove_node("hanna")
    assert "hanna" not in fl.nodes
    assert "hanna" not in fl.nodes["railo"].peers
    out = fl.swarm_query("topology check", "consensus")
    assert out["total_nodes"] == 2


def test_fleet_save_roundtrip() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / "sigma_swarm_fleet.json"
        fl = SigmaSwarmFleet()
        fl.add_node("n1", model="short", bias=0.1)
        fl.add_node("n2", model="verbose", bias=-0.05)
        fl.save(path)
        raw = json.loads(path.read_text(encoding="utf-8"))
        assert raw["version"] == 1
        fl2 = SigmaSwarmFleet.load(path)
        assert set(fl2.nodes) == {"n1", "n2"}
        assert fl2.nodes["n1"].model.style == "short"
        assert fl2.nodes["n1"].gate.bias == 0.1


def test_swarm_node_gate_bias_shapes_sigma() -> None:
    g_lo = SwarmNodeGate(bias=-0.25)
    g_hi = SwarmNodeGate(bias=0.25)
    s_lo, _ = g_lo.score("same", "resp")
    s_hi, _ = g_hi.score("same", "resp")
    assert s_lo < s_hi
