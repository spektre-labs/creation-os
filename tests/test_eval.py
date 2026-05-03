# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from pathlib import Path

from cos.eval.gemma_eval import run_truthfulqa_gemma_n
from cos.eval.halueval import fix_oracle_pairs, load_halueval_rows
from cos.eval.runner import run_all_benchmarks_lab, run_benchmark_row_stub
from cos.eval.truthfulqa import load_truthfulqa_rows


def test_fix_oracle_pairs_drops_empty() -> None:
    rows = fix_oracle_pairs([{"question": "", "label": 1}, {"question": "ok", "label": 0}])
    assert len(rows) == 1


def test_load_halueval_demo() -> None:
    rows = load_halueval_rows()
    assert len(rows) >= 1
    assert "expected" in rows[0]


def test_load_truthfulqa_demo() -> None:
    rows = load_truthfulqa_rows()
    assert "prompt" in rows[0]


def test_run_benchmark_stub() -> None:
    def model(prompt: str, ref: str = "") -> str:
        return "aligned" if "feathers" in prompt.lower() else "hallucination"

    out = run_benchmark_row_stub("HaluEval", model)
    assert "M_tier_behavioral" in out or "M_tier" in out


def test_run_all_benchmarks_lab() -> None:
    def model(prompt: str, ref: str = "") -> str:
        return "washington" if "president" in prompt.lower() else "stub"

    tbl = run_all_benchmarks_lab(model)
    assert "datasets" in tbl


def test_gemma_eval_dry_run_json(tmp_path: Path) -> None:
    out = tmp_path / "gem.json"
    payload = run_truthfulqa_gemma_n(dry_run=True, out_path=out)
    assert out.is_file()
    assert "records" in payload
