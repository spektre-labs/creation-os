# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import sys
from pathlib import Path
from typing import Any, Dict, List

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.openended import DiscoveryArchive, SigmaOpenEnded  # noqa: E402


class _MockGraph:
    """Minimal graph API matching :class:`SigmaOpenEnded` expectations."""

    def __init__(self, entities: List[str], rels_by_entity: Dict[str, List[Dict[str, Any]]]) -> None:
        self._entities = list(entities)
        self._rels = {k: list(v) for k, v in rels_by_entity.items()}

    def entities(self) -> List[str]:
        return list(self._entities)

    def relations_of(self, entity: str) -> List[Dict[str, Any]]:
        return list(self._rels.get(entity, []))


def test_scan_frontier_returns_uncertain() -> None:
    g = _MockGraph(
        ["Alice", "Bob"],
        {
            "Alice": [
                {
                    "subject": "Alice",
                    "relation": "knows",
                    "object": "Bob",
                    "sigma": 0.55,
                }
            ],
            "Bob": [],
        },
    )
    oe = SigmaOpenEnded(graph=g)
    f = oe.scan_frontier()
    assert len(f) == 1
    assert f[0]["entity"] == "Alice"
    assert f[0]["relation"] == "knows"
    assert f[0]["target"] == "Bob"
    assert 0.3 < f[0]["σ"] < 0.8


def test_discover_domain_from_seeds() -> None:
    g = _MockGraph(
        ["A", "B", "C"],
        {
            "A": [
                {"subject": "A", "relation": "to", "object": "B", "sigma": 0.4},
            ],
            "B": [
                {"subject": "B", "relation": "to", "object": "C", "sigma": 0.55},
            ],
            "C": [],
        },
    )
    oe = SigmaOpenEnded(graph=g)
    d = oe.discover_domain(["A"])
    assert d is not None
    assert set(d["entities"]) == {"A", "B", "C"}
    assert d["size"] == 3


def test_generate_exploration_goals() -> None:
    registered: List[str] = []

    class _Meta:
        def register_skill(self, name: str) -> None:
            registered.append(name)

    g = _MockGraph(
        ["X", "Y"],
        {
            "X": [
                {"subject": "X", "relation": "links", "object": "Y", "sigma": 0.5},
            ],
            "Y": [],
        },
    )
    oe = SigmaOpenEnded(graph=g, meta_goal=_Meta())
    goals = oe.generate_exploration_goals(n=3)
    assert len(goals) >= 1
    assert goals[0]["type"] == "explore"
    assert "X" in goals[0]["question"]
    assert registered and registered[0].startswith("explore_")


def test_novelty_score_known_low() -> None:
    g = _MockGraph(["alpha", "beta"], {"alpha": [], "beta": []})
    oe = SigmaOpenEnded(graph=g)
    n = oe.novelty_score("alpha beta gamma")
    assert n < 0.9


def test_novelty_score_unknown_high() -> None:
    g = _MockGraph(["alpha", "beta"], {"alpha": [], "beta": []})
    oe = SigmaOpenEnded(graph=g)
    n = oe.novelty_score("xyzzy quux zztop")
    assert n > 0.5


def test_explore_vs_exploit_decision() -> None:
    high = _MockGraph(
        ["e1", "e2"],
        {
            "e1": [
                {"subject": "e1", "relation": "r", "object": "e2", "sigma": 0.7},
            ],
            "e2": [],
        },
    )
    oe_hi = SigmaOpenEnded(graph=high)
    assert oe_hi.should_explore_or_exploit() == "explore"

    low = _MockGraph(
        ["e1", "e2"],
        {
            "e1": [
                {"subject": "e1", "relation": "r", "object": "e2", "sigma": 0.45},
            ],
            "e2": [],
        },
    )
    oe_lo = SigmaOpenEnded(graph=low)
    assert oe_lo.should_explore_or_exploit() == "exploit"


def test_empty_graph_defaults() -> None:
    oe = SigmaOpenEnded(graph=None)
    assert oe.scan_frontier() == []
    assert oe.novelty_score("anything") == 0.5
    assert oe.discover_domain(["x"]) is None
    assert oe.should_explore_or_exploit() == "explore"


# --- MAP-Elites-style :class:`DiscoveryArchive` ---


class _FixedSigmaGate:
    def __init__(self, sigma: float) -> None:
        self._σ = float(sigma)

    def score(self, _prompt: str, _response: str):
        return self._σ, "ACCEPT"


class _DecreasingSigmaGate:
    def __init__(self) -> None:
        self._calls = 0

    def score(self, _prompt: str, _response: str):
        self._calls += 1
        if self._calls == 1:
            return 0.82, "ACCEPT"
        return 0.14, "ACCEPT"


def test_attempt_new_cell_accepted() -> None:
    d = DiscoveryArchive(gate=_FixedSigmaGate(0.35), grid_size=1)
    r = d.attempt("solution-a", "behavior-a", "ctx")
    assert r["accepted"] is True
    assert r["σ"] == 0.35
    assert d.total_discoveries == 1


def test_attempt_better_sigma_replaces() -> None:
    d = DiscoveryArchive(gate=_DecreasingSigmaGate(), grid_size=1)
    r1 = d.attempt("first", "same_cell_beh")
    r2 = d.attempt("better", "same_cell_beh")
    assert r1["accepted"] is True
    assert r2["accepted"] is True
    assert d.archive[(0, 0)]["σ"] == 0.14
    assert d.total_discoveries == 1


def test_attempt_worse_rejected() -> None:
    d = DiscoveryArchive(gate=_FixedSigmaGate(0.2), grid_size=1)
    d.attempt("good", "beh")
    r = d.attempt("worse_solution", "beh")
    assert r["accepted"] is False
    assert "better" in r["reason"]


def test_novelty_decreases_with_density() -> None:
    gs = 8
    anc = DiscoveryArchive(gate=_FixedSigmaGate(0.25), grid_size=gs)
    anchor = "anchor_beh"
    c0 = DiscoveryArchive._behavior_to_cell(anchor, gs)
    n_lo = anc.attempt("s0", anchor)["novelty"]
    assert n_lo >= 0.99
    for i in range(4000):
        b = f"fill_{i:05d}"
        c = DiscoveryArchive._behavior_to_cell(b, gs)
        if c == c0:
            continue
        if max(abs(c[0] - c0[0]), abs(c[1] - c0[1])) <= 2:
            anc.attempt("fill", b, "")
    n_hi = anc.attempt("s1", anchor)["novelty"]
    assert n_hi < n_lo


def test_frontier_finds_empty_cells() -> None:
    d = DiscoveryArchive(gate=_FixedSigmaGate(0.3), grid_size=4)
    f0 = d.frontier()
    assert f0["total_empty"] == 16
    d.attempt("one", "beh_x")
    f1 = d.frontier()
    assert f1["total_empty"] == 15
    assert f1["coverage"] > 0


def test_explore_increases_coverage() -> None:
    d = DiscoveryArchive(gate=_FixedSigmaGate(0.33), grid_size=14)

    def gen(i: int, _frontier: dict) -> tuple[str, str]:
        return f"sol-{i}", f"beh-{i:06d}"

    out = d.explore(gen, n_attempts=120, context="lab")
    assert out["final_coverage"] > 0.0
    assert out["accepted"] + out["rejected"] == 120


def test_qd_score_increases() -> None:
    d = DiscoveryArchive(gate=_DecreasingSigmaGate(), grid_size=1)
    d.attempt("a", "b")
    q1 = d.quality_diversity_score()
    d.attempt("c", "b")
    q2 = d.quality_diversity_score()
    assert q2 > q1
