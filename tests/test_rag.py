# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from cos.rag import SigmaRAG
from cos.sigma_gate import SigmaGate


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
