# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Eval loaders and runners (lab stubs + JSONL ingest).

Canonical **M-tier disclosure** (incl. TruthfulQA saturation + HaluEval 0.514) lives in
``cos.bench.default_mtier_rows``. This package wires harness-shaped loaders; archive JSON per
``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

from cos.eval.checkpoint_eval import CheckpointEval, iter_checkpoint_results, load_checkpoint_meta
from cos.eval.dynamic import generate_questions, hash_question_set, run_dynamic_eval, verify_answers
from cos.eval.facts_grounding import load_facts, run_facts_grounding_eval
from cos.eval.metrics import auroc_binary, sm_ece_binary, snr_sigma_separation
from cos.eval.multi_model_eval import (
    EVAL_MODELS,
    MultiModelEval,
    compute_metrics_from_result_rows,
    format_r_ml_markdown_table,
    load_eval_tuples,
    run_one_model_checkpointed,
)
from cos.eval.runner import run_all_benchmarks_lab, run_benchmark_row_stub
from cos.eval.saturation import check_contamination, saturation_label
from cos.eval.simpleqa import load_simpleqa, run_simpleqa_eval

__all__ = [
    "EVAL_MODELS",
    "CheckpointEval",
    "MultiModelEval",
    "auroc_binary",
    "check_contamination",
    "compute_metrics_from_result_rows",
    "format_r_ml_markdown_table",
    "generate_questions",
    "hash_question_set",
    "iter_checkpoint_results",
    "load_checkpoint_meta",
    "load_eval_tuples",
    "load_facts",
    "load_simpleqa",
    "run_all_benchmarks_lab",
    "run_benchmark_row_stub",
    "run_dynamic_eval",
    "run_facts_grounding_eval",
    "run_one_model_checkpointed",
    "run_simpleqa_eval",
    "saturation_label",
    "sm_ece_binary",
    "snr_sigma_separation",
    "verify_answers",
]
