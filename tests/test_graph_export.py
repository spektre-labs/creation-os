# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import json
from pathlib import Path

from cos.graph import SigmaGraph
from cos.graph_export import GraphExport


def test_obsidian_creates_md_files(tmp_path: Path) -> None:
    g = SigmaGraph()
    g.add("Alpha", "rel", "Beta", sigma=0.15)
    d = tmp_path / "vault"
    GraphExport(g).to_obsidian(d)
    assert (d / "INDEX.md").is_file()
    assert any(d.glob("*.md"))


def test_obsidian_wiki_links(tmp_path: Path) -> None:
    g = SigmaGraph()
    g.add("One", "to", "Two", sigma=0.15)
    d = tmp_path / "v"
    GraphExport(g).to_obsidian(d)
    one_md = d / "One.md"
    assert one_md.is_file()
    body = one_md.read_text(encoding="utf-8")
    assert "[[" in body


def test_json_export_structure(tmp_path: Path) -> None:
    g = SigmaGraph()
    g.add("x", "y", "z", sigma=0.1)
    p = tmp_path / "g.json"
    data = GraphExport(g).to_json(p)
    assert "entities" in data and "triples" in data and "stats" in data
    loaded = json.loads(p.read_text(encoding="utf-8"))
    assert loaded["stats"]["triples"] == 1


def test_stats_counts_entities_triples() -> None:
    g = SigmaGraph()
    g.add("a", "r", "b", sigma=0.1)
    st = GraphExport(g).stats()
    assert st["entities"] == 2 and st["triples"] == 1
    assert "avg_sigma" in st


def test_high_sigma_triples_counted() -> None:
    g = SigmaGraph(write_threshold=0.99)
    g.add("u", "r", "v", sigma=0.8)
    st = GraphExport(g).stats()
    assert st["high_sigma_triples"] >= 1
