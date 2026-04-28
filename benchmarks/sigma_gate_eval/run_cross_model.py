#!/usr/bin/env python3
"""
Cross-generator smoke test (NOT full representation transfer).

The bundled LSD probe runs **GPT-2** hidden states on the **question** only and
uses MiniLM on the answer string (see ``FeatureExtractor.extract_trajectory_features``).
So this script answers: if we **generate** text with model B but still score with
the GPT-2-based gate, does ranking vs. a cheap wrong label remain non-random?

It does **not** run the probe heads on TinyLlama hidden tensors. A true
``probe-on-model-A apply-to-model-B`` path would require retraining or a shared
representation pipeline.

Requires: venv + PYTHONPATH=python.
"""
from __future__ import annotations

import argparse
import csv
import json
import math
import sys
from pathlib import Path
from typing import Any, Dict, List

import numpy as np

try:
    from sklearn.metrics import average_precision_score, roc_auc_score
except ImportError as e:
    print("sklearn required:", e, file=sys.stderr)
    sys.exit(2)


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--probe",
        type=Path,
        default=Path("benchmarks/sigma_gate_lsd/results_holdout/sigma_gate_lsd.pkl"),
    )
    ap.add_argument(
        "--csv",
        type=Path,
        default=Path("benchmarks/sigma_gate_lsd/splits/holdout.csv"),
    )
    ap.add_argument(
        "--generator-model",
        default="TinyLlama/TinyLlama-1.1B-Chat-v1.0",
        help="HF model used only for greedy decoding.",
    )
    ap.add_argument("--limit", type=int, default=40, help="Max rows (TinyLlama is slow).")
    ap.add_argument("--max-new-tokens", type=int, default=80)
    args = ap.parse_args()

    repo = _repo_root()
    sys.path.insert(0, str(repo / "python"))

    import torch
    from transformers import AutoModelForCausalLM, AutoTokenizer

    from cos.sigma_gate import SigmaGate

    probe = (repo / args.probe).resolve() if not args.probe.is_absolute() else args.probe.resolve()
    csv_path = (repo / args.csv).resolve() if not args.csv.is_absolute() else args.csv.resolve()
    if not probe.is_file() or not csv_path.is_file():
        print("Need probe pickle and CSV.", file=sys.stderr)
        sys.exit(2)

    rows: List[Dict[str, str]] = []
    with csv_path.open(newline="", encoding="utf-8") as fp:
        for row in csv.DictReader(fp):
            rows.append(row)
    if args.limit is not None:
        rows = rows[: int(args.limit)]

    gate = SigmaGate(str(probe))
    try:
        tokenizer = AutoTokenizer.from_pretrained(args.generator_model)
        if tokenizer.pad_token is None:
            tokenizer.pad_token = tokenizer.eos_token
        model = AutoModelForCausalLM.from_pretrained(
            args.generator_model,
            torch_dtype=torch.float32,
        )
        model.eval()

        results: List[Dict[str, Any]] = []
        for i, row in enumerate(rows):
            question = (row.get("question") or "").strip()
            reference = (row.get("best_answer") or "").strip()
            if not question:
                continue
            inputs = tokenizer(question, return_tensors="pt")
            with torch.no_grad():
                out = model.generate(
                    **inputs,
                    max_new_tokens=int(args.max_new_tokens),
                    do_sample=False,
                    pad_token_id=tokenizer.pad_token_id,
                )
            response = tokenizer.decode(
                out[0][inputs["input_ids"].shape[1] :],
                skip_special_tokens=True,
            ).strip()
            sigma, decision = gate(model, tokenizer, question, response)
            ref_l = reference.lower().strip()
            resp_l = response.lower().strip()
            correct = bool(ref_l) and (ref_l in resp_l or resp_l in ref_l)
            wrong = 0 if correct else 1
            results.append(
                {
                    "id": row.get("id", f"cross-{i:04d}"),
                    "sigma": float(sigma),
                    "decision": decision,
                    "correct": correct,
                    "wrong": wrong,
                }
            )
            print(f"[{len(results)}/{len(rows)}] sigma={sigma:.3f} {decision} gen={args.generator_model[:24]}...")

        sigmas = np.array([r["sigma"] for r in results], dtype=np.float64)
        labels = np.array([r["wrong"] for r in results], dtype=np.int64)
        if len(np.unique(labels)) < 2:
            auroc = float("nan")
            pr_auc = float("nan")
        else:
            auroc = float(roc_auc_score(labels, sigmas))
            pr_auc = float(average_precision_score(labels, sigmas))

        summary = {
            "eval_type": "cross_generator_smoke",
            "caveat": "Probe backbone is still GPT-2 + MiniLM from pickle; generator is separate.",
            "generator_model": args.generator_model,
            "n": len(results),
            "auroc_wrong_vs_sigma": auroc,
            "pr_auc_average_precision": pr_auc,
        }
        out_dir = repo / "benchmarks" / "sigma_gate_eval" / "results_cross_model"
        out_dir.mkdir(parents=True, exist_ok=True)
        (out_dir / "cross_model_summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
        print(json.dumps(summary, indent=2))
        if not math.isnan(auroc) and auroc >= 0.7:
            print("Note: AUROC>=0.7 here does NOT prove representation-level cross-model transfer.")
    finally:
        gate.close()


if __name__ == "__main__":
    main()
