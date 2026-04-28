#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
HaluEval **QA oracle pairs** (``pminervini/HaluEval`` ``qa`` config): score correct vs
hallucinated answers with the TruthfulQA-trained LSD gate, then AUROC with automatic
monotonicity repair (negated scores if raw AUROC ``< 0.5``).

Requires: ``datasets``, ``torch``, ``transformers``, ``sklearn``, ``sentence-transformers``
(for ``semantic_labeling``). Network on first dataset pull.

Set ``HF_TOKEN`` / ``HUGGING_FACE_HUB_TOKEN`` only in your shell — never commit tokens.
See ``docs/CLAIM_DISCIPLINE.md``.
"""
from __future__ import annotations

import argparse
import json
import pickle
import sys
from pathlib import Path


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def _pick_probe(repo: Path) -> Path:
    for rel in (
        "benchmarks/sigma_gate_lsd/results_holdout/sigma_gate_lsd.pkl",
        "benchmarks/sigma_gate_lsd/results_full/sigma_gate_lsd.pkl",
    ):
        p = repo / rel
        if p.is_file():
            return p
    raise FileNotFoundError("No sigma_gate_lsd.pkl in results_holdout or results_full")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--model", default="gpt2")
    ap.add_argument("--limit", type=int, default=50)
    ap.add_argument("--no-prism", action="store_true")
    args = ap.parse_args()

    repo = _repo_root()
    sys.path.insert(0, str(repo / "python"))
    eval_dir = Path(__file__).resolve().parent
    sys.path.insert(0, str(eval_dir))

    from run_cross_domain import eval_halueval_qa_oracle_pairs

    from transformers import AutoModelForCausalLM, AutoTokenizer

    from cos.sigma_gate import SigmaGate

    probe_path = _pick_probe(repo)
    with probe_path.open("rb") as f:
        probe = pickle.load(f)
    gate = SigmaGate(probe)

    tok = AutoTokenizer.from_pretrained(args.model)
    if tok.pad_token is None:
        tok.pad_token = tok.eos_token
    model = AutoModelForCausalLM.from_pretrained(args.model)
    model.eval()

    auroc, results, err = eval_halueval_qa_oracle_pairs(
        gate,
        model,
        tok,
        int(args.limit),
        use_prism=not bool(args.no_prism),
    )
    out_dir = repo / "benchmarks" / "sigma_gate_eval" / "results_halueval_oracle"
    out_dir.mkdir(parents=True, exist_ok=True)
    summary = {
        "auroc_wrong_vs_sigma": auroc,
        "n_rows": len(results),
        "error": err or None,
        "probe": str(probe_path.relative_to(repo)),
    }
    (out_dir / "halueval_oracle_summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print(json.dumps(summary, indent=2))
    return 0 if not err else 1


if __name__ == "__main__":
    raise SystemExit(main())
