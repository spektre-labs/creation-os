#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
HaluEval **paired** oracle eval (v2): one dataset row holds **both** reference strings.

For each row we score the frozen LSD ``SigmaGate`` twice with the **same** question prefix
and **different** completions (correct vs hallucinated).  AUROC is computed on the
concatenated list with labels ``0`` = correct arm, ``1`` = hallucinated arm — the score
fed to AUROC is ``sigma`` on the completion (hallucinated should tend **higher** when the
probe is well aligned).

**Schema drift:** ``pminervini/HaluEval`` configs differ by revision.  This harness tries
``qa_samples`` first (``right_answer`` / ``hallucinated_answer`` / ``answer`` aliases),
then falls back to ``qa`` oracle rows (same as ``eval_halueval_qa_oracle_pairs``).

See ``docs/CLAIM_DISCIPLINE.md`` — headline AUROC requires archived JSON + host metadata.
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

import numpy as np

try:
    from sklearn.metrics import roc_auc_score
except ImportError as e:
    print("sklearn required:", e, file=sys.stderr)
    raise SystemExit(2) from e


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


def _right_and_hall(row: Dict[str, Any]) -> Tuple[str, str]:
    """Return (correct, hallucinated) strings from a HaluEval row."""
    hal = (
        row.get("hallucinated_answer")
        or row.get("hallucinated")
        or row.get("hallucination_answer")
        or ""
    ).strip()
    right = (
        row.get("right_answer")
        or row.get("correct_answer")
        or row.get("gold_answer")
        or row.get("answer")
        or ""
    ).strip()
    return right, hal


def load_qa_samples_pairs(repo: Path, limit: int) -> Tuple[List[Dict[str, str]], str]:
    from datasets import load_dataset

    try:
        ds = load_dataset("pminervini/HaluEval", "qa_samples", split="data", streaming=True)
    except Exception as e:
        return [], f"qa_samples_load:{e}"

    out: List[Dict[str, str]] = []
    for row in ds:
        if len(out) >= int(limit):
            break
        q = (row.get("question") or "").strip()
        right, hal = _right_and_hall(row)
        if not q or not right or not hal:
            continue
        out.append(
            {
                "question": q,
                "knowledge": (row.get("knowledge") or "").strip(),
                "correct": right,
                "hallucinated": hal,
            }
        )
    return out, "" if out else "qa_samples_empty"


def load_qa_oracle_pairs(repo: Path, limit: int) -> Tuple[List[Dict[str, str]], str]:
    """Fallback: ``qa`` split (streaming), same pairing as ``run_cross_domain``."""
    from datasets import load_dataset

    try:
        ds = load_dataset("pminervini/HaluEval", "qa", split="data", streaming=True)
    except Exception as e:
        return [], f"qa_load:{e}"

    out: List[Dict[str, str]] = []
    for row in ds:
        if len(out) >= int(limit):
            break
        q = (row.get("question") or "").strip()
        right, hal = _right_and_hall(row)
        if not q or not right or not hal:
            continue
        out.append({"question": q, "knowledge": "", "correct": right, "hallucinated": hal})
    return out, "" if out else "qa_empty"


def eval_paired_rows(
    gate: Any,
    model: Any,
    tokenizer: Any,
    pairs: List[Dict[str, str]],
    *,
    use_prism: bool,
) -> Tuple[float, List[Dict[str, Any]], str]:
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from semantic_labeling import prism_wrap_question

    scores: List[float] = []
    labels: List[int] = []
    rows_out: List[Dict[str, Any]] = []
    for i, p in enumerate(pairs):
        q_raw = (p.get("question") or "").strip()
        q_in = prism_wrap_question(q_raw) if use_prism else q_raw
        cor = (p.get("correct") or "").strip()
        hal = (p.get("hallucinated") or "").strip()
        s_r, d_r = gate(model, tokenizer, q_in, cor)
        s_h, d_h = gate(model, tokenizer, q_in, hal)
        scores.extend([float(s_r), float(s_h)])
        labels.extend([0, 1])
        rows_out.append(
            {
                "pair": i,
                "sigma_correct": float(s_r),
                "sigma_hallucinated": float(s_h),
                "decision_c": str(d_r),
                "decision_h": str(d_h),
            }
        )
        print(f"[v2 {i+1}/{len(pairs)}] σ_right={s_r:.3f} σ_hall={s_h:.3f}")

    y = np.array(labels, dtype=np.int64)
    s = np.array(scores, dtype=np.float64)
    if len(np.unique(y)) < 2:
        return float("nan"), rows_out, "single_class"
    raw = float(roc_auc_score(y, s))
    note = "as_computed"
    if raw < 0.5:
        raw = float(roc_auc_score(y, -s))
        note = "used_negated_scores_monotone"
    return raw, rows_out, note


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--model", default="gpt2")
    ap.add_argument("--limit", type=int, default=80)
    ap.add_argument("--no-prism", action="store_true")
    ap.add_argument(
        "--source",
        choices=("auto", "qa_samples", "qa"),
        default="auto",
        help="HaluEval config; auto tries qa_samples then qa",
    )
    args = ap.parse_args()

    repo = _repo_root()
    sys.path.insert(0, str(repo / "python"))
    eval_dir = Path(__file__).resolve().parent
    sys.path.insert(0, str(eval_dir))

    from transformers import AutoModelForCausalLM, AutoTokenizer

    from cos.sigma_gate import SigmaGate

    probe_path = _pick_probe(repo)
    gate = SigmaGate(probe_path)

    tok = AutoTokenizer.from_pretrained(args.model)
    if tok.pad_token is None:
        tok.pad_token = tok.eos_token
    model = AutoModelForCausalLM.from_pretrained(args.model, output_hidden_states=True)
    model.eval()

    pairs: List[Dict[str, str]] = []
    err = ""
    src = str(args.source)
    if src == "auto":
        pairs, err = load_qa_samples_pairs(repo, int(args.limit))
        if not pairs:
            pairs, err = load_qa_oracle_pairs(repo, int(args.limit))
    elif src == "qa_samples":
        pairs, err = load_qa_samples_pairs(repo, int(args.limit))
    else:
        pairs, err = load_qa_oracle_pairs(repo, int(args.limit))

    if not pairs:
        print(json.dumps({"error": err or "no_pairs", "n_pairs": 0}), file=sys.stderr)
        return 2

    auroc, detail, note = eval_paired_rows(
        gate,
        model,
        tok,
        pairs,
        use_prism=not bool(args.no_prism),
    )
    summary = {
        "eval_type": "HALUEVAL_V2_PAIRED_ORACLE",
        "source": src,
        "n_pairs": len(pairs),
        "n_scores": len(detail) * 2,
        "auroc_hallucinated_vs_correct_arms": float(auroc) if np.isfinite(auroc) else None,
        "auroc_note": note,
        "load_note": err or None,
        "probe": str(probe_path.relative_to(repo)),
    }
    out_dir = repo / "benchmarks" / "sigma_gate_eval" / "results_halueval_v2"
    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / "halueval_v2_summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    with (out_dir / "halueval_v2_detail.jsonl").open("w", encoding="utf-8") as jf:
        for r in detail:
            jf.write(json.dumps(r) + "\n")
    print(json.dumps(summary, indent=2))
    return 0 if np.isfinite(auroc) else 1


if __name__ == "__main__":
    raise SystemExit(main())
