# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from pathlib import Path
from typing import Any, Tuple

from cos.rag import SigmaRAG
from cos.sigma_gate import SigmaGate


class _LowSigmaGate:
    """Deterministic low-σ gate for persisted-chunk tests (avoid entropy luck)."""

    def score(self, _prompt: str, _response: str) -> Tuple[float, str]:
        return 0.05, "ACCEPT"

    def compute_sigma(self, *_a: Any, **_k: Any) -> float:
        return 0.05


class _DropMarkerGate:
    """Marks chunks containing ``DROP_MARKER`` as high-σ at ingest / score time."""

    def score(self, _prompt: str, response: str) -> Tuple[float, str]:
        if "DROP_MARKER" in str(response):
            return 0.99, "ABSTAIN"
        return 0.05, "ACCEPT"

    def compute_sigma(self, *_a: Any, **_k: Any) -> float:
        return 0.05


def test_ingest_creates_chunks(tmp_path: Path) -> None:
    r = SigmaRAG(gate=_LowSigmaGate(), store_dir=tmp_path)
    text = " ".join(["word"] * 500)
    out = r.ingest(text, source="doc.txt", chunk_size=100, overlap=20)
    assert out["chunks_added"] >= 4
    assert out["total"] == len(r.chunks)


def test_chunk_overlap(tmp_path: Path) -> None:
    r = SigmaRAG(gate=_LowSigmaGate(), store_dir=tmp_path)
    words = [f"w{i}" for i in range(50)]
    text = " ".join(words)
    r.ingest(text, chunk_size=10, overlap=2)
    assert len(r.chunks) >= 2
    c0 = str(r.chunks[0]["text"]).split()
    c1 = str(r.chunks[1]["text"]).split()
    assert len(set(c0) & set(c1)) >= 2


def test_retrieve_returns_relevant(tmp_path: Path) -> None:
    r = SigmaRAG(gate=_LowSigmaGate(), store_dir=tmp_path)
    text = "Paris is the capital of France. London is the capital of the UK."
    r.ingest(text, source="capitals.txt", chunk_size=50, overlap=10)
    out = r.retrieve("Paris France capital", top_k=2, max_σ=0.8)
    assert any("Paris" in str(c.get("text", "")) for c in out)


def test_retrieve_filters_high_sigma(tmp_path: Path) -> None:
    r = SigmaRAG(gate=_DropMarkerGate(), store_dir=tmp_path)
    ok = " ".join(["keep"] * 80)
    bad = "DROP_MARKER must raise chunk sigma " + " ".join(["x"] * 40)
    r.ingest(ok + " " + bad, source="mixed.txt", chunk_size=40, overlap=8)
    hi = sum(1 for c in r.chunks if float(c.get("σ", 0.0)) > 0.8)
    assert hi >= 1
    out = r.retrieve("keep DROP_MARKER", top_k=5, max_σ=0.8)
    assert not any("DROP_MARKER" in str(c.get("text", "")) for c in out)


def test_query_returns_context(tmp_path: Path) -> None:
    r = SigmaRAG(gate=_LowSigmaGate(), store_dir=tmp_path)
    r.ingest("Paris is beautiful in spring.", source="p.txt", chunk_size=30, overlap=5)
    result = r.query("What about Paris?", top_k=2)
    assert result.get("context") and "Paris" in str(result["context"])
    assert int(result.get("context_chunks", 0)) >= 1


def test_empty_store_abstains(tmp_path: Path) -> None:
    r = SigmaRAG(gate=_LowSigmaGate(), store_dir=tmp_path)
    assert not r.chunks
    result = r.query("anything")
    assert result["verdict"] == "ABSTAIN"
    assert result["context_chunks"] == 0
    assert result.get("answer") is None


def test_stats(tmp_path: Path) -> None:
    r = SigmaRAG(gate=_LowSigmaGate(), store_dir=tmp_path)
    r.ingest("x " * 100, source="s.txt", chunk_size=20, overlap=4)
    s = r.stats()
    assert s["total_chunks"] >= 1
    assert "s.txt" in s["sources"]


def test_retrieve_returns_sigma() -> None:
    r = SigmaRAG()
    r.index_documents(["Paris is France.", "London is UK."])
    out = r.retrieve("capital France", 2)
    assert len(out) == 2
    assert "sigma" in out[0]


def test_sigma_rerank_filters() -> None:
    r = SigmaRAG(sigma_keep_below=0.99)
    g = SigmaGate()
    chunks = ["a", "Paris is the capital of France"]
    rr = r.sigma_rerank("France capital?", chunks, g)
    assert "kept" in rr and "sorted" in rr


def test_hybrid_rrf() -> None:
    r = SigmaRAG()
    r.index_documents(["x y z", "a b c", "x y"])
    h = r.hybrid_search("x y", top_n=4)
    assert "ids" in h and len(h["ids"]) >= 1


def test_semantic_chunk() -> None:
    r = SigmaRAG()
    doc = "First sentence. Second sentence! Third stays."
    chunks = r.semantic_chunk(doc, cosine_threshold=0.5)
    assert len(chunks) >= 1


def test_augment_contains_query() -> None:
    r = SigmaRAG()
    aug = r.augment("Q?", [{"chunk": "ctx"}])
    assert "Q?" in aug and "ctx" in aug


def test_generate_stub() -> None:
    r = SigmaRAG()
    g = SigmaGate()

    class M:
        def generate(self, p: str) -> str:
            return "ok"

    out = r.generate("p", M(), g)
    assert "text" in out and "sigma" in out


def test_pipeline_end_to_end() -> None:
    r = SigmaRAG()

    class M:
        def generate(self, p: str) -> str:
            return "4"

    corp = ["2+2 equals four.", "Unrelated poetry."]
    out = r.pipeline("What is 2+2?", corp, M())
    assert "generate" in out


def test_cosine_helper() -> None:
    r = SigmaRAG()
    u = r._sent_vec("a")
    v = r._sent_vec("a")
    assert r._cos(u, v) > 0.9
