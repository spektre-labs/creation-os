# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import sys
from pathlib import Path
from typing import Any, Dict, List

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.openended import SigmaOpenEnded  # noqa: E402


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
