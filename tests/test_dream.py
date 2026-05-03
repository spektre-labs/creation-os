# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import time
from pathlib import Path

from cos.dream import DreamCycle, run_dream_maintenance
from cos.graph import SigmaGraph, Triple


def _inject(g: SigmaGraph, subj: str, rel: str, obj: str, *, sigma: float = 0.2, ts: float | None = None) -> None:
    t = Triple(subj, rel, obj, sigma=sigma, timestamp=ts if ts is not None else time.time())
    g.triples[t.id] = t
    g._index_add(t)


def test_deduplicate_merges_similar() -> None:
    g = SigmaGraph()
    g.add("a b", "r1", "o1", sigma=0.2)
    g.add("b a", "r2", "o2", sigma=0.2)
    dc = DreamCycle(g)
    dc.deduplicate(threshold=0.93)
    assert dc.report["deduped"] >= 1
    assert len(g.triples) <= 2


def test_decay_increases_sigma_for_old() -> None:
    g = SigmaGraph(write_threshold=0.99)
    _inject(g, "a", "rel", "b", sigma=0.1, ts=1.0)
    tid = next(iter(g.triples.keys()))
    before = float(g.triples[tid].sigma)
    dc = DreamCycle(g)
    dc.decay(max_age_days=0.0)
    assert dc.report["decayed"] >= 1
    assert g.triples[tid].sigma > before


def test_infer_relations_sigma_gates(monkeypatch) -> None:
    g = SigmaGraph()
    g.add("A", "via", "M", sigma=0.15)
    g.add("M", "via", "B", sigma=0.15)

    from cos.sigma_gate import SigmaGate

    calls: list[str] = []

    def _accept(self, prompt, response, reference=None):
        calls.append("x")
        return 0.1, SigmaGate.ACCEPT

    monkeypatch.setattr(SigmaGate, "score", _accept)
    dc = DreamCycle(g)
    dc.infer_relations(max_inferences=5)
    assert dc.report["inferred"] >= 1
    assert g.has_edge("A", "B")

    g2 = SigmaGraph()
    g2.add("X", "via", "Y", sigma=0.15)
    g2.add("Y", "via", "Z", sigma=0.15)

    def _abstain(self, prompt, response, reference=None):
        return 1.0, SigmaGate.ABSTAIN

    monkeypatch.setattr(SigmaGate, "score", _abstain)
    dc2 = DreamCycle(g2)
    dc2.infer_relations(max_inferences=5)
    assert not g2.has_edge("X", "Z")


def test_cleanup_removes_orphans() -> None:
    g = SigmaGraph()
    g.add("solo", "self", "solo", sigma=0.2)
    assert len(g.entities()) == 1
    dc = DreamCycle(g)
    dc.cleanup_orphans()
    assert dc.report["orphans"] >= 1
    assert len(g.triples) == 0


def test_insights_finds_hubs() -> None:
    g = SigmaGraph()
    hub = "HubEntity"
    for i in range(5):
        g.add(hub, f"links{i}", f"N{i}", sigma=0.1)
    dc = DreamCycle(g)
    dc.generate_insights(top_n=5)
    assert any("Hub:" in s for s in dc.report["insights"])


def test_full_cycle_returns_report() -> None:
    g = SigmaGraph()
    g.add("p", "q", "r", sigma=0.2)
    rep = DreamCycle(g).run(max_inferences=0)
    assert set(rep.keys()) >= {"deduped", "decayed", "inferred", "orphans", "insights"}


def test_run_dream_maintenance_bundle() -> None:
    g = SigmaGraph()
    _inject(g, "s", "t", "old", sigma=0.35, ts=1.0)
    out = run_dream_maintenance(g, run_infer=False, max_age_days=0.0)
    assert "report" in out and "sigma_after_maintenance" in out
    assert out["sigma_after_maintenance"]["triples"] == 0


def test_dream_graph_export_roundtrip(tmp_path: Path) -> None:
    from cos.graph_export import GraphExport, load_graph_from_json

    g = SigmaGraph()
    g.add("u", "r", "v", sigma=0.2)
    DreamCycle(g).run(max_inferences=0)
    p = tmp_path / "post_dream.json"
    GraphExport(g).to_json(p)
    g2 = SigmaGraph()
    load_graph_from_json(p, g2)
    assert len(g2.triples) >= 1
