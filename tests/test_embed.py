# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from cos.embed import SigmaEmbed


def test_embed_sigma_shape() -> None:
    e = SigmaEmbed(dim=8)
    out = e.embed_sigma("hello world")
    assert len(out["embedding"]) == 8
    assert len(out["sigma_dims"]) == 8


def test_similarity() -> None:
    e = SigmaEmbed(dim=4)
    a = e.embed_sigma("a")
    b = e.embed_sigma("b")
    s = e.similarity_with_sigma(
        a["embedding"],
        b["embedding"],
        a["sigma_dims"],
        b["sigma_dims"],
    )
    assert "cosine" in s and "sigma_confidence" in s


def test_cluster() -> None:
    e = SigmaEmbed()
    embs = [[0.1, 0.9], [0.2, 0.8], [0.9, 0.1]]
    c = e.cluster_sigma(embs, k=2)
    assert len(c["labels"]) == 3


def test_drift() -> None:
    e = SigmaEmbed()
    old = [[1.0, 0.0], [1.0, 0.0]]
    new = [[0.0, 1.0], [0.0, 1.0]]
    d = e.drift_detection(old, new)
    assert d["shift_sigma"] >= 0.0


def test_rag_rerank() -> None:
    e = SigmaEmbed()
    r = e.rag_rerank_chunks("q", ["long unrelated", "q match short"])
    assert r[0]["index"] in (0, 1)


def test_custom_embedder() -> None:
    e = SigmaEmbed(dim=4)

    def emb(t: str) -> list[float]:
        return [float(len(t)) / 10.0] * 4

    out = e.embed_sigma("abc", embedder=emb)
    assert len(out["embedding"]) == 4
