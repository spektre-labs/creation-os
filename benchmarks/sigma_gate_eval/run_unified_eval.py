#!/usr/bin/env python3
"""
Evaluate ``SigmaGateUnified`` on the same holdout CSV protocol as ``run_holdout_eval.py``.

Writes ``benchmarks/sigma_gate_eval/results_unified/unified_holdout_summary.json``
(does not overwrite ``results_holdout/holdout_summary.json``).
"""
from __future__ import annotations

import argparse
import csv
import json
import pickle
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


def _eval_dir() -> Path:
    return Path(__file__).resolve().parent


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--probe", type=Path, default=Path("benchmarks/sigma_gate_lsd/results_holdout/sigma_gate_lsd.pkl"))
    ap.add_argument("--holdout-csv", type=Path, default=Path("benchmarks/sigma_gate_lsd/splits/holdout.csv"))
    ap.add_argument("--generator-model", default="gpt2")
    ap.add_argument("--max-new-tokens", type=int, default=100)
    ap.add_argument("--labeling", choices=("semantic", "substring"), default="semantic")
    ap.add_argument("--semantic-threshold", type=float, default=0.45)
    ap.add_argument("--no-prism", action="store_true")
    ap.add_argument("--w-probe", type=float, default=0.6)
    ap.add_argument("--w-spectral", type=float, default=0.2)
    ap.add_argument("--w-logprob", type=float, default=0.2)
    args = ap.parse_args()

    repo = _repo_root()
    sys.path.insert(0, str(repo / "python"))
    sys.path.insert(0, str(_eval_dir()))

    import torch
    from transformers import AutoModelForCausalLM, AutoTokenizer

    from cos.sigma_gate_unified import SigmaGateUnified

    try:
        from semantic_labeling import is_correct_semantic, prism_wrap_question
    except ImportError as e:
        if args.labeling == "semantic" or not args.no_prism:
            print("Install sentence-transformers for semantic/PRISM paths:", e, file=sys.stderr)
            sys.exit(3)
        is_correct_semantic = None  # type: ignore[assignment]

        def prism_wrap_question(q: str) -> str:  # noqa: F811
            return (q or "").strip()

    if args.labeling == "semantic" and is_correct_semantic is None:
        print("semantic labeling requires sentence-transformers", file=sys.stderr)
        sys.exit(3)

    probe = (repo / args.probe).resolve() if not args.probe.is_absolute() else args.probe.resolve()
    csv_path = (repo / args.holdout_csv).resolve() if not args.holdout_csv.is_absolute() else args.holdout_csv.resolve()
    if not probe.is_file() or not csv_path.is_file():
        print("Missing probe or CSV", file=sys.stderr)
        sys.exit(2)

    training_cv = float("nan")
    try:
        training_cv = float(pickle.loads(probe.read_bytes())["manifest"].get("cv_auroc_mean", float("nan")))
    except Exception:
        pass

    rows: List[Dict[str, str]] = []
    with csv_path.open(newline="", encoding="utf-8") as fp:
        rows.extend(csv.DictReader(fp))

    gate = SigmaGateUnified(
        probe,
        weights=(float(args.w_probe), float(args.w_spectral), float(args.w_logprob)),
    )
    try:
        tok = AutoTokenizer.from_pretrained(args.generator_model)
        if tok.pad_token is None:
            tok.pad_token = tok.eos_token
        model = AutoModelForCausalLM.from_pretrained(
            args.generator_model,
            output_hidden_states=True,
            output_attentions=True,
        )
        model.eval()

        results: List[Dict[str, Any]] = []
        for i, row in enumerate(rows):
            question = (row.get("question") or "").strip()
            reference = (row.get("best_answer") or "").strip()
            if not question:
                continue
            prompt_for_model = question if args.no_prism else prism_wrap_question(question)
            inputs = tok(prompt_for_model, return_tensors="pt")
            dev = next(model.parameters()).device
            inputs = {k: v.to(dev) for k, v in inputs.items()}
            with torch.no_grad():
                out = model.generate(
                    **inputs,
                    max_new_tokens=int(args.max_new_tokens),
                    do_sample=False,
                    pad_token_id=tok.pad_token_id,
                )
            response = tok.decode(out[0][inputs["input_ids"].shape[1] :], skip_special_tokens=True).strip()
            sigma, decision = gate(model, tok, prompt_for_model, response, reference=reference)
            if args.labeling == "semantic":
                correct, sim = is_correct_semantic(response, reference, threshold=float(args.semantic_threshold))
            else:
                ref_l = reference.lower().strip()
                resp_l = response.lower().strip()
                correct = bool(ref_l) and (ref_l in resp_l or resp_l in ref_l)
                sim = None
            wrong = 0 if correct else 1
            results.append(
                {
                    "id": row.get("id", f"holdout-{i:04d}"),
                    "sigma_unified": float(sigma),
                    "decision": decision,
                    "correct": correct,
                    "wrong": wrong,
                    "label_sim": sim,
                }
            )
            mark = "ok" if correct else "x"
            print(f"[{len(results)}/{len(rows)}] sigma_u={sigma:.3f} {decision} {mark}")

        sigmas = np.array([r["sigma_unified"] for r in results], dtype=np.float64)
        labels = np.array([r["wrong"] for r in results], dtype=np.int64)
        if len(np.unique(labels)) < 2:
            auroc, pr_auc = float("nan"), float("nan")
        else:
            auroc = float(roc_auc_score(labels, sigmas))
            pr_auc = float(average_precision_score(labels, sigmas))

        summary = {
            "eval_type": "UNIFIED_HOLDOUT",
            "generator_model": args.generator_model,
            "weights_probe_spectral_logprob": [float(args.w_probe), float(args.w_spectral), float(args.w_logprob)],
            "n": len(results),
            "auroc_wrong_vs_sigma_unified": auroc,
            "pr_auc_average_precision": pr_auc,
            "training_cv_auroc_manifest": training_cv,
            "probe_path": str(probe),
            "holdout_csv": str(csv_path),
            "labeling": str(args.labeling),
        }

        out_dir = repo / "benchmarks" / "sigma_gate_eval" / "results_unified"
        out_dir.mkdir(parents=True, exist_ok=True)
        (out_dir / "unified_holdout_summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
        with (out_dir / "unified_holdout_detail.jsonl").open("w", encoding="utf-8") as jf:
            for r in results:
                jf.write(json.dumps(r) + "\n")

        print(f"\n  UNIFIED holdout AUROC: {auroc:.4f}  PR-AUC: {pr_auc:.4f}  n={len(results)}")
    finally:
        gate.close()


if __name__ == "__main__":
    main()
