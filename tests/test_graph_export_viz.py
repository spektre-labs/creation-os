# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import json
from pathlib import Path

import pytest

from cos.graph import SigmaGraph
from cos.graph_export import (
    export_json,
    export_obsidian,
    load_graph_from_json,
    triples_to_sigma_edges,
)


def test_triples_to_sigma_edges_has_reliability() -> None:
    g = SigmaGraph()
    g.add("A", "to", "B", sigma=0.25)
    edges = triples_to_sigma_edges(g)
    assert edges and "reliability" in edges[0]


def test_export_import_roundtrip(tmp_path: Path) -> None:
    g = SigmaGraph()
    g.add("U", "r", "V", sigma=0.2)
    jp = tmp_path / "g.json"
    export_json(g, jp)
    g2 = SigmaGraph()
    load_graph_from_json(jp, g2)
    assert len(g2.triples) == 1


def test_export_obsidian_writes_files(tmp_path: Path) -> None:
    g = SigmaGraph()
    g.add("Alpha", "beta", "Gamma", sigma=0.2)
    d = tmp_path / "vault"
    out = export_obsidian(g, d)
    assert out["files_written"] >= 1
    assert any(d.glob("*.md"))


def test_json_payload_structure(tmp_path: Path) -> None:
    g = SigmaGraph()
    g.add("x", "y", "z", sigma=0.1)
    p = tmp_path / "out.json"
    export_json(g, p)
    data = json.loads(p.read_text(encoding="utf-8"))
    assert "sigma_edges" in data and "triples" in data


def test_visualize_optional_dep(tmp_path: Path) -> None:
    pytest.importorskip("networkx")
    pytest.importorskip("matplotlib")
    from cos.graph_viz import visualize

    g = SigmaGraph()
    g.add("n1", "e", "n2", sigma=0.2)
    png = tmp_path / "x.png"
    out = visualize(g, png)
    assert Path(out["path"]).is_file()
