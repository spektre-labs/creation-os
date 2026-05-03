# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from pathlib import Path

from cos.bench import (
    HALUEVAL_EXAMPLE_AUROC,
    MTIER_BENCHMARK_KEYS,
    SigmaBench,
    claim_discipline_bundle,
    default_mtier_rows,
)
from cos.eval.dynamic import generate_questions, hash_question_set
from cos.eval.saturation import saturation_label
from cos.report import SigmaReport


def test_mtier_table_includes_all_benchmarks() -> None:
    rows = default_mtier_rows()
    ids = [str(r["id"]) for r in rows]
    for key in MTIER_BENCHMARK_KEYS:
        assert key in ids
    names = {str(r["benchmark"]) for r in rows}
    assert "TruthfulQA MC" in names
    assert "HaluEval QA" in names
    assert "SimpleQA" in names
    assert "FACTS Grounding" in names
    assert "FaithDial" in names
    assert "HaluEval 2.0" in names
    assert "Custom dynamic" in names


def test_saturation_label_truthfulqa() -> None:
    assert "saturated" in saturation_label("TruthfulQA MC").lower()


def test_saturation_label_simpleqa() -> None:
    assert "current" in saturation_label("SimpleQA").lower()


def test_evidence_ladder_includes_negatives() -> None:
    b = SigmaBench()
    lad = b.evidence_ladder([])
    assert lad["not_agi_achieved"] is True
    negs = lad.get("negatives") or []
    assert len(negs) >= 1
    assert any("0.514" in str(x) for x in negs)
    blob = claim_discipline_bundle()
    assert blob["not_agi_achieved"] is True
    assert blob["evidence_ladder_includes_negatives"] is True
    assert HALUEVAL_EXAMPLE_AUROC == 0.514


def test_dynamic_bench_different_each_run() -> None:
    a = generate_questions("science", "hard", 3, seed=1)
    b = generate_questions("science", "hard", 3, seed=2)
    assert hash_question_set(a) != hash_question_set(b)
    assert a[0]["question"] != b[0]["question"]


def test_claim_discipline_no_false_positives() -> None:
    doc = Path(__file__).resolve().parents[1] / "docs" / "CLAIM_DISCIPLINE.md"
    text = doc.read_text(encoding="utf-8")
    assert "NOT AGI ACHIEVED" in text
    assert "0.514" in text
    assert "saturated" in text.lower()
    assert "What we claim" in text
    assert "What we do NOT claim" in text
    md = SigmaReport().mtier_table_markdown(default_mtier_rows())
    assert "TruthfulQA MC" in md
    assert "HaluEval QA" in md
    assert "fail" in md.lower()
    assert "saturated" in md.lower()
