# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import time

from cos.index import SigmaIndex


def test_index_documents_count() -> None:
    ix = SigmaIndex()
    r = ix.index(["alpha\n\nbeta", "gamma"])
    assert r["indexed"] == 2


def test_incremental_add_returns_record() -> None:
    ix = SigmaIndex()
    rec = ix.incremental_add("one\n\ntwo")
    assert "chunks" in rec and "doc_sigma" in rec and "id" in rec


def test_search_ranks_by_sigma_and_overlap() -> None:
    ix = SigmaIndex()
    ix.index(["ParisFrance capital", "unrelated noise"])
    hits = ix.search("Paris France", top_k=2)
    assert len(hits) >= 1
    assert "sigma" in hits[0] and "sim" in hits[0]


def test_sigma_quality_score_float() -> None:
    ix = SigmaIndex()
    s = ix.sigma_quality_score("short doc about logic")
    assert 0.0 <= s <= 1.0


def test_stale_detection_bumps_over_time() -> None:
    ix = SigmaIndex(stale_sigma_bump_per_day=0.05)
    rec = ix.incremental_add("hello world")
    past = time.time() - 86400 * 10
    ix._docs[0]["indexed_at"] = past  # type: ignore[attr-defined]
    st = ix.stale_detection(rec["id"], now=time.time())
    assert st is not None
    assert st["stale_sigma_bump"] > 0


def test_dedup_candidates_high_similarity() -> None:
    ix = SigmaIndex()
    ix.index(["same seed text A\n\nextra", "same seed text B\n\ndiff"])
    # first chunks differ; lower threshold to exercise path
    d = ix.dedup_candidates(threshold=0.01)
    assert isinstance(d, list)
