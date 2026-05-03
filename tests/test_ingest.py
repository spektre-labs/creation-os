# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from pathlib import Path

import pytest

from cos.graph import SigmaGraph
from cos.graph_export import GraphExport, load_graph_from_json
from cos.ingest import SigmaIngest, chunk_text, chunk_words, extract_triples_regex, ingest, load_document_text


def test_chunk_text_non_empty() -> None:
    chunks = chunk_text("abcdefgh" * 20, max_chars=40, overlap=5)
    assert len(chunks) >= 2


def test_extract_triples_regex() -> None:
    rows = extract_triples_regex("Paris capital France.\nLondon capital UK.")
    assert any(r[0].lower() == "paris" for r in rows)


def test_load_document_text_md(tmp_path: Path) -> None:
    p = tmp_path / "a.md"
    p.write_text("Line one.\n", encoding="utf-8")
    assert "Line" in load_document_text(p)


def test_load_document_html(tmp_path: Path) -> None:
    p = tmp_path / "a.html"
    p.write_text("<html><body><p>Hi</p></body></html>", encoding="utf-8")
    assert "Hi" in load_document_text(p)


def test_ingest_txt_adds_triple(tmp_path: Path) -> None:
    p = tmp_path / "t.txt"
    p.write_text("Earth orbits Sun.", encoding="utf-8")
    out = ingest(p)
    assert out["path"] == str(p.resolve())
    assert "sigma_per_triple" in out
    assert len(out["sigma_per_triple"]) >= 1


def test_ingest_skips_abstain_rows_have_flag(tmp_path: Path, monkeypatch) -> None:
    from cos import sigma_gate as sg

    p = tmp_path / "t.txt"
    p.write_text("X y Z.", encoding="utf-8")

    def _abstain_score(self, prompt, response, reference=None):
        return 1.0, "ABSTAIN"

    monkeypatch.setattr(sg.SigmaGate, "score", _abstain_score)
    out = ingest(p)
    assert out["skipped_abstain"] >= 1


def test_ingest_txt_extracts_triples(tmp_path: Path) -> None:
    p = tmp_path / "k.txt"
    p.write_text("Alice is a engineer. Bob created Tool.", encoding="utf-8")
    g = SigmaGraph()
    rep = SigmaIngest(g).ingest(p, chunk_size=500)
    assert rep["accepted"] >= 1
    assert len(rep["triples"]) >= 1


def test_ingest_rejects_high_sigma(tmp_path: Path, monkeypatch) -> None:
    from cos import sigma_gate as sg

    p = tmp_path / "h.txt"
    p.write_text("Carol is a analyst.", encoding="utf-8")

    def _high(self, prompt, response, reference=None):
        return 0.95, sg.SigmaGate.RETHINK

    monkeypatch.setattr(sg.SigmaGate, "score", _high)
    g = SigmaGraph()
    rep = SigmaIngest(g).ingest(p)
    assert rep["rejected"] >= 1
    assert len(g.triples) == 0


def test_regex_extraction_finds_relations() -> None:
    chunk = "Eve created AcmeCorp."
    s = SigmaIngest(SigmaGraph())
    triples = s._extract_regex(chunk)
    assert any(t[1] == "created" for t in triples)


def test_unsupported_format_raises(tmp_path: Path) -> None:
    p = tmp_path / "x.xyz"
    p.write_text("nope", encoding="utf-8")
    with pytest.raises(ValueError, match="Unsupported"):
        SigmaIngest(SigmaGraph()).ingest(p)


def test_chunk_splits_text() -> None:
    parts = chunk_words("a " * 50, size=10)
    assert len(parts) >= 5
    assert all(len(p.split()) <= 10 for p in parts)


def test_report_counts_accepted_rejected(tmp_path: Path) -> None:
    p = tmp_path / "r.txt"
    p.write_text("Pat is a teacher. Quinn is a student.", encoding="utf-8")
    g = SigmaGraph()
    rep = SigmaIngest(g).ingest(p, chunk_size=100)
    assert rep["accepted"] + rep["rejected"] == len(rep["triples"])


def test_ingest_after_load_json_accumulates(tmp_path: Path) -> None:
    base = tmp_path / "base.json"
    g0 = SigmaGraph()
    g0.add("Seed", "has", "Value", sigma=0.15)
    GraphExport(g0).to_json(base)
    g = SigmaGraph()
    load_graph_from_json(base, g)
    doc = tmp_path / "extra.txt"
    doc.write_text("More is a detail.", encoding="utf-8")
    SigmaIngest(g).ingest(doc, chunk_size=500)
    assert len(g.triples) >= 2
