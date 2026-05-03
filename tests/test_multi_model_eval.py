# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from cos.eval.checkpoint_eval import CheckpointEval, iter_checkpoint_results
from cos.eval.multi_model_eval import (
    MultiModelEval,
    compute_metrics_from_result_rows,
    format_r_ml_markdown_table,
    load_eval_tuples,
)
from cos.sigma_gate import SigmaGate


def test_eval_single_model_mock() -> None:
    gate = SigmaGate()

    def gen(_m: str, prompt: str) -> str:
        if "president" in prompt.lower():
            return "George Washington was the first president."
        return "I am not sure."

    me = MultiModelEval(gate, endpoint="http://127.0.0.1:0", generate_fn=gen)
    rows = load_eval_tuples("truthfulqa")
    out = me.eval_model("gemma3-4b", rows, n=4, benchmark_label="truthfulqa")
    assert out["n"] == len(rows)
    assert "auroc" in out and out["rows"]


def test_eval_all_models_mock() -> None:
    gate = SigmaGate()

    def gen(_m: str, prompt: str) -> str:
        return "Paris" if "france" in prompt.lower() else "maybe"

    me = MultiModelEval(gate, endpoint="http://127.0.0.1:0", generate_fn=gen)
    rows = load_eval_tuples("truthfulqa")[:3]
    bundle = me.eval_all(rows, n=3, models=("gemma3-4b", "qwen3.6-a3b"), benchmark_label="truthfulqa")
    assert len(bundle["models"]) == 2
    assert "gemma3-4b" in bundle["models"] and "qwen3.6-a3b" in bundle["models"]


def test_mtier_table_format() -> None:
    md = format_r_ml_markdown_table(
        [
            {
                "model": "gemma3-4b",
                "auroc": 0.812,
                "abstention_rate": 0.05,
                "smece": 0.11,
                "snr": 1.2,
                "benchmark": "TruthfulQA",
                "status": "⚠ saturated",
            },
        ],
        include_known_halu=True,
    )
    assert "HaluEval" in md
    assert "0.514" in md
    assert "gemma3-4b" in md
    assert "NOT AGI" in md


def test_checkpoint_save_and_resume(tmp_path) -> None:
    ck = CheckpointEval(tmp_path)
    pairs = [("p0", "e0"), ("p1", "e1")]

    def eval_fn(i: int, pair: tuple[str, str]):
        return {
            "index": i,
            "pair": list(pair),
            "sigma": 0.3,
            "verdict": "ACCEPT",
            "correct": True,
        }

    rid = "xyz"
    out = ck.run_with_checkpoint(eval_fn, pairs, 2, run_id=rid, checkpoint_every=1, meta={"model_key": "m", "benchmark": "truthfulqa"})
    assert len(out) == 2
    assert ck.checkpoint_path(rid).is_file()


def test_checkpoint_survives_simulated_crash(tmp_path) -> None:
    ck = CheckpointEval(tmp_path)
    pairs = [("p0", "e0"), ("p1", "e1")]
    calls: list[int] = []

    def eval_fn(i: int, pair: tuple[str, str]):
        calls.append(i)
        return {
            "index": i,
            "sigma": 0.4 + 0.1 * i,
            "verdict": "ACCEPT",
            "correct": i % 2 == 0,
            "pair": list(pair),
        }

    rid = "crash"
    ck.run_with_checkpoint(eval_fn, pairs, 2, run_id=rid, checkpoint_every=1, meta={"model_key": "gemma3-4b", "benchmark": "truthfulqa"})
    path = ck.checkpoint_path(rid)
    lines = path.read_text(encoding="utf-8").splitlines()
    path.write_text(lines[0] + "\n", encoding="utf-8")

    def eval_fn_resume(i: int, pair: tuple[str, str]):
        calls.append(i + 10)
        return {
            "index": i,
            "sigma": 0.9,
            "verdict": "RETHINK",
            "correct": False,
            "pair": list(pair),
            "resumed": True,
        }

    merged = ck.continue_from_path(path, eval_fn_resume, pairs, 2, checkpoint_every=1)
    assert len(merged) == 2
    assert merged[1].get("resumed") is True
    assert len(iter_checkpoint_results(path)) == 2


def test_metrics_include_auroc_abstention_smece_snr() -> None:
    rows = [
        {"sigma": 0.2, "verdict": "ACCEPT", "correct": True},
        {"sigma": 0.9, "verdict": "ABSTAIN", "correct": False},
        {"sigma": 0.45, "verdict": "RETHINK", "correct": True},
    ]
    m = compute_metrics_from_result_rows(rows)
    for k in ("auroc", "abstention_rate", "smece", "snr", "n"):
        assert k in m
    assert m["abstention_rate"] > 0
