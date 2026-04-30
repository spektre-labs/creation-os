#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
ICR + multi-layer HIDE + LSD + entropy **fusion** on TruthfulQA-style holdout rows.

Scores the **same** greedy completion with:
  * ``SigmaGate`` (LSD),
  * ``multi_layer_hide`` (lab HIDE L4–L10),
  * ``icr_sigma_from_model_forward`` (ICR-style cross-layer residual),
  * ``SigmaPrecheck`` entropy (prompt-only),
  * ``fused_sigma`` (weighted mean or max).

See ``docs/CLAIM_DISCIPLINE.md`` — harness observables only.
"""
from __future__ import annotations

import argparse
import csv
import json
import math
import sys
from pathlib import Path
from typing import Any, Dict, List, Tuple

import numpy as np

try:
    from sklearn.metrics import average_precision_score, roc_auc_score
except ImportError as e:
    print("sklearn required:", e, file=sys.stderr)
    raise SystemExit(2) from e


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def _eval_dir() -> Path:
    return Path(__file__).resolve().parent


def _pick_probe(repo: Path) -> Path:
    for rel in (
        "benchmarks/sigma_gate_lsd/results_holdout/sigma_gate_lsd.pkl",
        "benchmarks/sigma_gate_lsd/results_full/sigma_gate_lsd.pkl",
    ):
        p = repo / rel
        if p.is_file():
            return p
    raise FileNotFoundError("No sigma_gate_lsd.pkl in results_holdout or results_full")


def _auroc_with_optional_flip(labels: np.ndarray, scores: np.ndarray) -> Tuple[float, float, str]:
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


def _pack_channel_auroc(
    labels: np.ndarray,
    results: List[Dict[str, Any]],
    col: str,
) -> Dict[str, Any]:
    s = np.array([float(r[col]) for r in results], dtype=np.float64)
    au, raw, note = _auroc_with_optional_flip(labels, s)
    if len(np.unique(labels)) < 2:
        pr = float("nan")
    else:
        pr_s = -s if note.startswith("used_negated") else s
        pr = float(average_precision_score(labels, pr_s))
    return {
        "auroc": _json_float(float(au)),
        "auroc_raw": _json_float(float(raw)),
        "auroc_note": note,
        "pr_auc": _json_float(float(pr)),
    }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--model", default="gpt2")
    ap.add_argument("--probe", type=Path, default=None)
    ap.add_argument(
        "--holdout-csv",
        type=Path,
        default=Path("benchmarks/sigma_gate_lsd/splits/holdout.csv"),
    )
    ap.add_argument("--holdout-limit", type=int, default=57)
    ap.add_argument("--max-new-tokens", type=int, default=100)
    ap.add_argument("--semantic-threshold", type=float, default=0.45)
    ap.add_argument("--no-prism", action="store_true")
    ap.add_argument(
        "--fusion-mode",
        choices=("weighted_mean", "max"),
        default="weighted_mean",
        help="``fused_sigma`` combiner",
    )
    ap.add_argument(
        "--fusion-weights-json",
        type=Path,
        default=None,
        help="optional JSON object str→float overriding DEFAULT_WEIGHTS keys",
    )
    args = ap.parse_args()

    repo = _repo_root()
    sys.path.insert(0, str(repo / "python"))
    sys.path.insert(0, str(_eval_dir()))

    import torch
    from transformers import AutoModelForCausalLM, AutoTokenizer

    from cos.sigma_fusion import DEFAULT_WEIGHTS, fused_sigma, merge_weight_dict
    from cos.sigma_gate import SigmaGate
    from cos.sigma_gate_precheck import SigmaPrecheck
    from cos.sigma_hide import multi_layer_hide
    from cos.sigma_icr import icr_sigma_from_model_forward

    try:
        from semantic_labeling import is_correct_semantic, prism_wrap_question
    except ImportError as e:
        print("semantic_labeling:", e, file=sys.stderr)
        return 3

    probe_path = (repo / args.probe).resolve() if args.probe else _pick_probe(repo)
    csv_path = (repo / args.holdout_csv).resolve() if not args.holdout_csv.is_absolute() else args.holdout_csv
    if not probe_path.is_file():
        print(f"Missing probe: {probe_path}", file=sys.stderr)
        return 2
    if not csv_path.is_file():
        print(f"Missing CSV: {csv_path}", file=sys.stderr)
        return 2

    fusion_weights = DEFAULT_WEIGHTS
    if args.fusion_weights_json and args.fusion_weights_json.is_file():
        fusion_weights = merge_weight_dict(
            DEFAULT_WEIGHTS,
            json.loads(args.fusion_weights_json.read_text(encoding="utf-8")),
        )

    rows: List[Dict[str, str]] = []
    with csv_path.open(newline="", encoding="utf-8") as fp:
        for row in csv.DictReader(fp):
            rows.append(row)
    lim = max(0, int(args.holdout_limit))
    if lim:
        rows = rows[:lim]

    tok = AutoTokenizer.from_pretrained(args.model)
    if tok.pad_token is None:
        tok.pad_token = tok.eos_token
    model = AutoModelForCausalLM.from_pretrained(args.model, output_hidden_states=True)
    model.eval()

    gate = SigmaGate(str(probe_path))
    precheck = SigmaPrecheck(tau_skip=0.99, mode="entropy")
    use_prism = not bool(args.no_prism)

    results: List[Dict[str, Any]] = []
    try:
        for i, row in enumerate(rows):
            question = (row.get("question") or "").strip()
            reference = (row.get("best_answer") or "").strip()
            if not question:
                continue
            prompt_for_model = prism_wrap_question(question) if use_prism else question
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
            text_full = f"{prompt_for_model}\n{response}".strip()

            s_lsd, dec = gate(model, tok, prompt_for_model, response)
            s_hide, _dh = multi_layer_hide(model, tok, prompt_for_model, response, max_length=512)
            s_icr, _di = icr_sigma_from_model_forward(model, tok, text_full, max_length=512)
            _sg, s_ent, _det = precheck.predict(model, tok, prompt_for_model)

            sigs = {
                "lsd": float(s_lsd),
                "hide": float(s_hide),
                "icr": float(s_icr),
                "entropy": float(s_ent),
            }
            s_fuse = fused_sigma(sigs, fusion_weights, mode=str(args.fusion_mode))

            correct, sim = is_correct_semantic(
                response,
                reference,
                threshold=float(args.semantic_threshold),
            )
            wrong = 0 if correct else 1
            results.append(
                {
                    "id": row.get("id", f"holdout-{i:04d}"),
                    "wrong": wrong,
                    "sigma_lsd": sigs["lsd"],
                    "sigma_hide_ml": sigs["hide"],
                    "sigma_icr": sigs["icr"],
                    "sigma_entropy": sigs["entropy"],
                    "sigma_fusion": float(s_fuse),
                    "lsd_decision": str(dec),
                    "label_sim": sim,
                }
            )
            print(
                f"[{len(results)}/{len(rows)}] fuse={s_fuse:.3f} "
                f"lsd={sigs['lsd']:.3f} hide={sigs['hide']:.3f} icr={sigs['icr']:.3f} wrong={wrong}"
            )

        labels = np.array([r["wrong"] for r in results], dtype=np.int64)

        summary: Dict[str, Any] = {
            "eval_type": "ICR_MULTI_HIDE_LSD_ENTROPY_FUSION_HOLDOUT",
            "hf_model": str(args.model),
            "n_rows": len(results),
            "n_wrong": int((labels == 1).sum()),
            "n_correct": int((labels == 0).sum()),
            "fusion_mode": str(args.fusion_mode),
            "fusion_weights": fusion_weights,
            "probe": str(probe_path.relative_to(repo)) if str(probe_path).startswith(str(repo)) else str(probe_path),
            "holdout_csv": str(csv_path.relative_to(repo)) if str(csv_path).startswith(str(repo)) else str(csv_path),
            "prism_prompt": use_prism,
        }
        for key, col in (
            ("lsd", "sigma_lsd"),
            ("hide_ml", "sigma_hide_ml"),
            ("icr", "sigma_icr"),
            ("entropy", "sigma_entropy"),
            ("fusion", "sigma_fusion"),
        ):
            m = _pack_channel_auroc(labels, results, col)
            summary[f"auroc_wrong_vs_{key}"] = m["auroc"]
            summary[f"auroc_raw_{key}"] = m["auroc_raw"]
            summary[f"auroc_note_{key}"] = m["auroc_note"]
            summary[f"pr_auc_{key}"] = m["pr_auc"]

        out_dir = repo / "benchmarks" / "sigma_gate_eval" / "results_icr"
        out_dir.mkdir(parents=True, exist_ok=True)
        (out_dir / "icr_summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
        with (out_dir / "icr_detail.jsonl").open("w", encoding="utf-8") as jf:
            for r in results:
                jf.write(json.dumps(r) + "\n")

        fu = summary.get("auroc_wrong_vs_fusion")
        print("\n" + "=" * 50)
        print(json.dumps({"auroc_fusion": fu, "n_rows": len(results)}, indent=2))
        print("=" * 50)
        print(f"Wrote {out_dir / 'icr_summary.json'}")
    finally:
        gate.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
