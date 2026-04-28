#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
Gemma-2-2B-IT + **HIDE (lab / training-free)** on TruthfulQA-style holdout rows.

Scores the **same** greedy completion with ``SigmaHIDE(backend="lab")`` (forward HSIC
sketch on pooled hidden states) — no LSD probe retraining. Semantic **wrong** labels
use MiniLM cosine vs ``best_answer`` (same threshold as ``run_holdout_eval.py``).

Requires: ``HF_TOKEN`` or ``HUGGING_FACE_HUB_TOKEN`` in the environment for gated
downloads (never commit tokens). Optional: ``sentence-transformers``, ``torch``,
``transformers``, ``accelerate`` (required for ``device_map="auto"`` on multi-device / Gemma),
``scikit-learn``.

See ``docs/CLAIM_DISCIPLINE.md`` — this harness is **not** the shipped GPT-2 LSD AUROC.
"""
from __future__ import annotations

import argparse
import csv
import json
import math
import os
import sys
from pathlib import Path
from typing import Any, Dict, List, Tuple

import numpy as np

try:
    from sklearn.metrics import average_precision_score, roc_auc_score
except ImportError as e:
    print("sklearn required:", e, file=sys.stderr)
    sys.exit(2)


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def _eval_sigma_gate_eval() -> Path:
    return _repo_root() / "benchmarks" / "sigma_gate_eval"


def _format_chat_prompt(tokenizer: Any, user_text: str) -> str:
    if hasattr(tokenizer, "apply_chat_template") and getattr(tokenizer, "chat_template", None):
        chat = [{"role": "user", "content": (user_text or "").strip()}]
        return str(
            tokenizer.apply_chat_template(
                chat,
                tokenize=False,
                add_generation_prompt=True,
            )
        )
    return (user_text or "").strip()


def _auroc_with_optional_flip(labels: np.ndarray, scores: np.ndarray) -> Tuple[float, float, str]:
    """Return (auroc_used, auroc_raw, note). If raw < 0.5, use negated scores (equivalent to 1-AUROC swap)."""
    if len(np.unique(labels)) < 2:
        return float("nan"), float("nan"), "single_class_labels"
    raw = float(roc_auc_score(labels, scores))
    if raw < 0.5:
        inv = float(roc_auc_score(labels, -scores))
        return inv, raw, "used_negated_scores_monotonic_wrong_vs_risk"
    return raw, raw, "as_computed"


def _json_float(x: float) -> Any:
    if isinstance(x, float) and (math.isnan(x) or math.isinf(x)):
        return None
    return x


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--hf-model", default="google/gemma-2-2b-it")
    ap.add_argument(
        "--holdout-csv",
        type=Path,
        default=Path("benchmarks/sigma_gate_lsd/splits/holdout.csv"),
    )
    ap.add_argument("--limit", type=int, default=10, help="max holdout rows (use <=10 if RAM < 8GB)")
    ap.add_argument("--max-new-tokens", type=int, default=100)
    ap.add_argument("--semantic-threshold", type=float, default=0.45)
    ap.add_argument("--no-prism", action="store_true")
    ap.add_argument(
        "--cpu",
        action="store_true",
        help="load in float32 on CPU (slow; prefer GPU + bfloat16 for Gemma-2)",
    )
    args = ap.parse_args()

    repo = _repo_root()
    sys.path.insert(0, str(repo / "python"))
    sys.path.insert(0, str(_eval_sigma_gate_eval()))

    import torch

    try:
        from transformers import AutoTokenizer, Gemma2ForCausalLM
    except ImportError as e:
        print("transformers with Gemma2ForCausalLM required:", e, file=sys.stderr)
        return 2

    from cos.sigma_hide import SigmaHIDE

    mid = str(args.hf_model).strip()
    if "gemma" not in mid.lower():
        raise RuntimeError(
            "This harness targets Gemma-2 (google/gemma-2-*-it). "
            "Pass --hf-model google/gemma-2-2b-it — no GPT-2 fallback."
        )
    token = (os.environ.get("HF_TOKEN") or os.environ.get("HUGGING_FACE_HUB_TOKEN") or "").strip()
    if not token:
        raise RuntimeError(
            "Set HF_TOKEN or HUGGING_FACE_HUB_TOKEN in the environment to download Gemma weights "
            "(never commit tokens to the repository)."
        )

    try:
        from semantic_labeling import is_correct_semantic, prism_wrap_question
    except ImportError as e:
        print("semantic_labeling / sentence-transformers:", e, file=sys.stderr)
        return 3

    csv_path = (repo / args.holdout_csv).resolve() if not args.holdout_csv.is_absolute() else args.holdout_csv
    if not csv_path.is_file():
        print(f"Missing CSV: {csv_path}", file=sys.stderr)
        return 2

    rows: List[Dict[str, str]] = []
    with csv_path.open(newline="", encoding="utf-8") as fp:
        for row in csv.DictReader(fp):
            rows.append(row)
    rows = rows[: max(0, int(args.limit))]
    if not rows:
        print("No holdout rows to evaluate.", file=sys.stderr)
        return 2

    tok = AutoTokenizer.from_pretrained(mid, token=token)
    if tok.pad_token is None:
        tok.pad_token = tok.eos_token

    use_mps = hasattr(torch.backends, "mps") and torch.backends.mps.is_available()
    if bool(args.cpu):
        dtype = torch.bfloat16
        model = Gemma2ForCausalLM.from_pretrained(
            mid,
            output_hidden_states=True,
            torch_dtype=dtype,
            token=token,
        )
        model = model.to(torch.device("cpu"))
    elif use_mps:
        dtype = torch.float32
        model = Gemma2ForCausalLM.from_pretrained(
            mid,
            output_hidden_states=True,
            torch_dtype=dtype,
            token=token,
        )
        model = model.to(torch.device("mps"))
    else:
        dtype = torch.bfloat16
        load_kw: Dict[str, Any] = {
            "pretrained_model_name_or_path": mid,
            "output_hidden_states": True,
            "torch_dtype": dtype,
            "token": token,
        }
        try:
            import accelerate  # noqa: F401

            load_kw["device_map"] = "auto"
        except ImportError as e:
            raise RuntimeError(
                "CUDA path needs `accelerate` for device_map='auto', or run on Mac with MPS "
                "(no accelerate required) or pass --cpu."
            ) from e
        try:
            model = Gemma2ForCausalLM.from_pretrained(**load_kw)
        except Exception as e:
            raise RuntimeError(f"Gemma load failed: {type(e).__name__}: {e}") from e
    model.eval()

    hide = SigmaHIDE(backend="lab")
    use_prism = not bool(args.no_prism)

    results: List[Dict[str, Any]] = []
    for i, row in enumerate(rows):
        question = (row.get("question") or "").strip()
        reference = (row.get("best_answer") or "").strip()
        if not question:
            continue
        raw_q = prism_wrap_question(question) if use_prism else question
        prompt_for_model = _format_chat_prompt(tok, raw_q)

        inputs = tok(prompt_for_model, return_tensors="pt")
        dev = next(model.parameters()).device
        inputs = {k: v.to(dev) for k, v in inputs.items()}
        in_len = int(inputs["input_ids"].shape[1])
        with torch.no_grad():
            out = model.generate(
                **inputs,
                max_new_tokens=int(args.max_new_tokens),
                do_sample=False,
                pad_token_id=tok.pad_token_id,
            )
        response = tok.decode(out[0, in_len:], skip_special_tokens=True).strip()

        sigma, det = hide.compute_sigma(
            model,
            tok,
            prompt_for_model,
            response,
            max_length=512,
        )
        correct, sim = is_correct_semantic(
            response,
            reference,
            threshold=float(args.semantic_threshold),
        )
        wrong = 0 if correct else 1
        results.append(
            {
                "id": row.get("id", f"holdout-{i:04d}"),
                "sigma_hide": float(sigma),
                "wrong": wrong,
                "correct": bool(correct),
                "label_sim": sim,
                "hide_backend": det.get("backend"),
            }
        )
        print(f"[{len(results)}/{len(rows)}] sigma_hide={sigma:.3f} wrong={wrong}")

    sigmas = np.array([r["sigma_hide"] for r in results], dtype=np.float64)
    labels = np.array([r["wrong"] for r in results], dtype=np.int64)
    auroc, auroc_raw, auroc_note = _auroc_with_optional_flip(labels, sigmas)
    if len(np.unique(labels)) < 2:
        pr_auc = float("nan")
    else:
        scores_for_pr = -sigmas if auroc_note.startswith("used_negated") else sigmas
        pr_auc = float(average_precision_score(labels, scores_for_pr))

    summary: Dict[str, Any] = {
        "eval_type": "GEMMA_HOLDOUT_HIDE_LAB",
        "hf_model": str(args.hf_model),
        "n_rows": len(results),
        "n_wrong": int((labels == 1).sum()),
        "n_correct": int((labels == 0).sum()),
        "auroc_wrong_vs_sigma_hide": _json_float(float(auroc)),
        "auroc_raw_before_monotone_fix": _json_float(float(auroc_raw)),
        "auroc_note": auroc_note,
        "pr_auc_average_precision": _json_float(float(pr_auc)),
        "semantic_threshold": float(args.semantic_threshold),
        "labeling": "semantic_cosine_minilm",
        "prism_prompt": use_prism,
        "holdout_csv": str(csv_path.relative_to(repo)) if str(csv_path).startswith(str(repo)) else str(csv_path),
    }

    out_dir = repo / "benchmarks" / "sigma_gate_scaling" / "results_gemma"
    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / "gemma_hide_summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    with (out_dir / "gemma_hide_detail.jsonl").open("w", encoding="utf-8") as jf:
        for r in results:
            jf.write(json.dumps(r) + "\n")

    print("\n" + "=" * 50)
    if isinstance(auroc, float) and math.isnan(auroc):
        print("  AUROC undefined (single-class labels or empty run).")
    else:
        print(f"  Gemma HIDE (lab) AUROC (wrong vs sigma_hide): {auroc:.4f}")
    print(f"  PR-AUC: {pr_auc:.4f}")
    print(f"  Wrote {out_dir / 'gemma_hide_summary.json'}")
    print("=" * 50)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except RuntimeError as e:
        print(str(e), file=sys.stderr)
        raise SystemExit(2)
