#!/usr/bin/env python3
"""
Per-layer **HIDE** sweep plus full TruthfulQA holdout / TriviaQA / HaluEval generative AUROC.

Uses ``cos.sigma_hide.SigmaHIDE`` (official upstream metric when KeyBERT is installed, else
lab forward HSIC). Clone the reference tree for diff review::

    git clone https://github.com/C-anwoy/HIDE.git external/hide

This harness is **not** a reproduction of paper-reported AUROC without a local calibration run.
"""
from __future__ import annotations

import argparse
import csv
import json
import math
import sys
from pathlib import Path
from typing import Any, Callable, Dict, List, Tuple

import numpy as np

try:
    from sklearn.metrics import roc_auc_score
except ImportError as e:
    print("sklearn required:", e, file=sys.stderr)
    sys.exit(2)


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def _eval_dir() -> Path:
    return Path(__file__).resolve().parent


def _auroc(labels: np.ndarray, scores: np.ndarray) -> float:
    if labels.size == 0 or len(np.unique(labels)) < 2:
        return float("nan")
    return float(roc_auc_score(labels, scores))


def _greedy(
    model: Any,
    tokenizer: Any,
    q_in: str,
    *,
    max_new_tokens: int,
    device: Any,
) -> str:
    import torch

    inputs = tokenizer(q_in, return_tensors="pt")
    inputs = {k: v.to(device) for k, v in inputs.items()}
    with torch.no_grad():
        out = model.generate(
            **inputs,
            max_new_tokens=int(max_new_tokens),
            do_sample=False,
            pad_token_id=tokenizer.pad_token_id,
        )
    return tokenizer.decode(
        out[0][inputs["input_ids"].shape[1] :],
        skip_special_tokens=True,
    ).strip()


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--model", default="gpt2")
    ap.add_argument(
        "--hide-backend",
        choices=("auto", "official", "lab"),
        default="auto",
        help="official uses upstream HIDE generate + KeyBERT HSIC when deps exist",
    )
    ap.add_argument("--keywords", type=int, default=20)
    ap.add_argument("--holdout-limit", type=int, default=0, help="0 = all holdout rows")
    ap.add_argument("--scan-limit", type=int, default=30, help="rows for per-layer AUROC scan")
    ap.add_argument(
        "--layer-scan",
        type=str,
        default="4,6,8,10",
        help="comma-separated layer indices (transformer block indices in hidden_states[0])",
    )
    ap.add_argument("--trivia-limit", type=int, default=100)
    ap.add_argument("--halu-limit", type=int, default=100)
    ap.add_argument("--max-new-tokens", type=int, default=100)
    ap.add_argument("--semantic-threshold", type=float, default=0.45)
    ap.add_argument("--no-prism", action="store_true")
    args = ap.parse_args()

    repo = _repo_root()
    sys.path.insert(0, str(repo / "python"))
    sys.path.insert(0, str(_eval_dir()))

    try:
        import datasets  # noqa: F401
    except ImportError:
        print("pip install datasets", file=sys.stderr)
        sys.exit(3)

    from datasets import load_dataset

    import torch
    from transformers import AutoModelForCausalLM, AutoTokenizer

    from cos.sigma_hide import SigmaHIDE

    try:
        from semantic_labeling import (
            cosine_similarity,
            is_correct_semantic,
            is_correct_vs_any_reference,
            prism_wrap_question,
        )
    except ImportError as e:
        print("semantic_labeling:", e, file=sys.stderr)
        sys.exit(3)

    use_prism = not bool(args.no_prism)
    scan_layers = [int(x.strip()) for x in str(args.layer_scan).split(",") if x.strip()]

    tok = AutoTokenizer.from_pretrained(args.model)
    if tok.pad_token is None:
        tok.pad_token = tok.eos_token
    model = AutoModelForCausalLM.from_pretrained(
        args.model,
        output_hidden_states=True,
        output_attentions=True,
    )
    model.eval()
    device = next(model.parameters()).device

    csv_path = repo / "benchmarks" / "sigma_gate_lsd" / "splits" / "holdout.csv"
    hold_rows: List[Dict[str, str]] = []
    with csv_path.open(newline="", encoding="utf-8") as fp:
        hold_rows = list(csv.DictReader(fp))
    if int(args.holdout_limit) > 0:
        hold_rows = hold_rows[: int(args.holdout_limit)]

    def _label_holdout(row: Dict[str, Any], response: str) -> Tuple[int, str]:
        ref = (row.get("best_answer") or "").strip()
        ok, _ = is_correct_semantic(response, ref, threshold=float(args.semantic_threshold))
        return (0 if ok else 1), "holdout"

    def _eval_hide_on_rows(
        hide: SigmaHIDE,
        name: str,
        rows: List[Dict[str, Any]],
        label_fn: Callable[..., Tuple[int, str]],
    ) -> float:
        scores: List[float] = []
        labels: List[int] = []
        for i, row in enumerate(rows):
            question = (row.get("question") or "").strip()
            if not question:
                continue
            q_in = prism_wrap_question(question) if use_prism else question
            response = _greedy(model, tok, q_in, max_new_tokens=int(args.max_new_tokens), device=device)
            wrong, _ = label_fn(row, response)
            labels.append(wrong)
            try:
                s, det = hide.compute_sigma(
                    model,
                    tok,
                    q_in,
                    response,
                    max_new_tokens=int(args.max_new_tokens),
                )
                scores.append(float(s))
                be = det.get("backend", "?")
            except Exception:
                scores.append(0.5)
                be = "err"
            mark = "ok" if wrong == 0 else "x"
            print(f"[{name} {i+1}/{len(rows)}] {mark} hide={scores[-1]:.3f} be={be}")
        y = np.array(labels, dtype=np.int64)
        return _auroc(y, np.array(scores, dtype=np.float64))

    # --- per-layer scan (subset) ---
    scan_rows = hold_rows[: max(1, int(args.scan_limit))]
    layer_auroc: Dict[str, float] = {}
    for lyr in scan_layers:
        hide = SigmaHIDE(backend=str(args.hide_backend), layer=int(lyr), keywords=int(args.keywords))
        layer_auroc[str(lyr)] = _eval_hide_on_rows(hide, f"scan_L{lyr}", scan_rows, _label_holdout)

    best_layer = None
    best_auroc = float("-inf")
    for lyr, v in layer_auroc.items():
        if not math.isnan(v) and v > best_auroc:
            best_auroc = v
            best_layer = int(lyr)
    if best_layer is None and scan_layers:
        best_layer = int(scan_layers[len(scan_layers) // 2])
    if best_layer is None:
        best_layer = 0

    hide_best = SigmaHIDE(backend=str(args.hide_backend), layer=int(best_layer), keywords=int(args.keywords))

    all_out: Dict[str, Any] = {
        "layer_scan_auroc_holdout_subset": layer_auroc,
        "chosen_layer": int(best_layer),
        "effective_backend": hide_best.effective_backend(),
    }

    all_out["truthfulqa_holdout"] = _eval_hide_on_rows(hide_best, "holdout_full", hold_rows, _label_holdout)

    try:
        ds = load_dataset(
            "trivia_qa",
            "unfiltered.nocontext",
            split="validation",
            streaming=True,
        )
        trows: List[Dict[str, Any]] = []
        for row in ds:
            if len(trows) >= int(args.trivia_limit):
                break
            q = (row.get("question") or "").strip()
            aliases = row.get("answer", {}).get("aliases") or []
            if not q or not aliases:
                continue
            trows.append({"question": q, "best_answer": str(aliases[0]), "aliases": aliases})

        def _label_trivia(row: Dict[str, Any], response: str) -> Tuple[int, str]:
            aliases = row.get("aliases") or []
            ok, _ = is_correct_vs_any_reference(
                response, [str(a) for a in aliases], threshold=float(args.semantic_threshold)
            )
            return (0 if ok else 1), "trivia"

        all_out["triviaqa"] = _eval_hide_on_rows(hide_best, "triviaqa", trows, _label_trivia)
    except Exception as e:
        all_out["triviaqa"] = {"error": str(e)}

    try:
        ds = load_dataset("pminervini/HaluEval", "qa_samples", split="data", streaming=True)
        hrows: List[Dict[str, Any]] = []
        for row in ds:
            if len(hrows) >= int(args.halu_limit):
                break
            question = (row.get("question") or "").strip()
            ref_answer = (row.get("answer") or "").strip()
            hal_raw = (row.get("hallucination") or "").strip().lower()
            if not question or not ref_answer or hal_raw not in ("yes", "no"):
                continue
            hrows.append(
                {
                    "question": question,
                    "best_answer": ref_answer,
                    "hallucination": hal_raw,
                    "ref_for_cos": ref_answer,
                }
            )

        def _label_halu(row: Dict[str, Any], response: str) -> Tuple[int, str]:
            ref = (row.get("ref_for_cos") or "").strip()
            hal_raw = (row.get("hallucination") or "").strip().lower()
            sim = cosine_similarity(response, ref)
            if hal_raw == "no":
                ok = sim > float(args.semantic_threshold)
            else:
                ok = sim <= float(args.semantic_threshold)
            return (0 if ok else 1), "halu"

        all_out["halueval_generative"] = _eval_hide_on_rows(hide_best, "halueval", hrows, _label_halu)
    except Exception as e:
        all_out["halueval_generative"] = {"error": str(e)}

    summary = {
        "generator_model": args.model,
        "hide_backend_requested": str(args.hide_backend),
        "keywords": int(args.keywords),
        "aurocs_wrong_vs_score": all_out,
        "disclaimer": "AUROC is harness-only; official HIDE path matches upstream func layout — calibrate before external claims.",
    }

    out_dir = repo / "benchmarks" / "sigma_gate_eval" / "results_hide"
    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / "hide_summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")

    print(f"\n{'=' * 70}")
    print("  HIDE eval (AUROC wrong vs sigma_hide)")
    print(f"{'=' * 70}")
    print(json.dumps(summary, indent=2))
    print(f"{'=' * 70}")


if __name__ == "__main__":
    main()
