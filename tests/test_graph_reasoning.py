# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.graph import SigmaGraph  # noqa: E402
from cos.sigma_gate import SigmaGate  # noqa: E402


def test_multi_hop_finds_path() -> None:
    g = SigmaGraph()
    g.add("A", "r1", "B", sigma=0.05)
    g.add("B", "r2", "C", sigma=0.05)
    out = g.multi_hop("A", "C", max_hops=4)
    assert out["found"]
    assert len(out["path"]) == 2
    assert out["hops"] == 2


def test_multi_hop_no_path() -> None:
    g = SigmaGraph()
    g.add("X", "r", "Y", sigma=0.1)
    out = g.multi_hop("X", "Z", max_hops=3)
    assert not out["found"]


def test_multi_hop_sigma_accumulates() -> None:
    g = SigmaGraph()
    g.add("a", "r", "b", sigma=0.1)
    g.add("b", "r", "c", sigma=0.2)
    out = g.multi_hop("a", "c", max_hops=4)
    assert out["found"]
    assert out["cumulative_sigma"] == round(0.3, 4) or abs(float(out["cumulative_sigma"]) - 0.3) < 1e-6
    assert float(out["avg_sigma"]) == round(0.3 / 2, 4)


def test_fol_query_by_subject() -> None:
    g = SigmaGraph()
    g.add("France", "capital", "Paris", sigma=0.04)
    g.add("France", "in", "Europe", sigma=0.12)
    rows = g.query_fol(subject="France", relation="capital")
    assert len(rows) == 1
    assert rows[0]["object"] == "Paris"


def test_fol_query_by_object() -> None:
    g = SigmaGraph()
    g.add("France", "capital", "Paris", sigma=0.04)
    rows = g.query_fol(relation="capital", obj="Paris")
    assert len(rows) == 1
    assert rows[0]["subject"] == "France"


def test_subgraph_extracts_neighborhood() -> None:
    g = SigmaGraph()
    g.add("A", "rel", "B", sigma=0.1)
    g.add("B", "rel", "C", sigma=0.1)
    g.add("C", "rel", "D", sigma=0.1)
    sub = g.subgraph("A", radius=2)
    assert sub["size"] >= 3
    assert len(sub["triples"]) >= 2
    labels = {x.lower() for x in sub["entities"]}
    assert "a" in labels and "b" in labels and "c" in labels


def test_reason_chain_accept() -> None:
    g = SigmaGraph()
    g.add("Tokyo", "capital_of", "Japan", sigma=0.05)
    gate = SigmaGate(threshold_accept=0.25, threshold_abstain=0.85)
    out = g.reason_chain("Japan Tokyo capital", gate)
    assert out["verdict"] == "ACCEPT"


def test_reason_chain_abstain_unknown() -> None:
    g = SigmaGraph()
    gate = SigmaGate()
    out = g.reason_chain("no entities like xyzzy zork", gate)
    assert out["verdict"] == "ABSTAIN"
    assert out.get("reason") == "No known entities in question"
