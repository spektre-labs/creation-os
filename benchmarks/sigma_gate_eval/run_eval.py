#!/usr/bin/env python3
"""
Full sigma-gate v4 evaluation on TruthfulQA 200 (or first N rows).

Reports AUROC (wrong vs sigma), accuracy on ACCEPT, abstention rate,
wrong-but-ACCEPT count, coverage, and a coarse ECE. Writes JSON artefacts under
``benchmarks/sigma_gate_eval/results/``.

Requires: repo venv with transformers/torch/sklearn (see benchmarks/sigma_gate_lsd/.venv).
Import path: prepend ``python/`` to PYTHONPATH for ``from cos.sigma_gate import SigmaGate``.
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
    from sklearn.metrics import roc_auc_score
except ImportError as e:
    print("sklearn required:", e, file=sys.stderr)
    sys.exit(2)


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def _ece_binary(y: np.ndarray, p: np.ndarray, n_bins: int = 10) -> float:
    """Expected calibration error for binary y (1=positive class) with scores p in [0,1]."""
    y = np.asarray(y, dtype=np.float64)
    p = np.asarray(p, dtype=np.float64)
    bins = np.linspace(0.0, 1.0, n_bins + 1)
    ece = 0.0
    n = len(y)
    if n == 0:
        return 0.0
    for lo, hi in zip(bins[:-1], bins[1:]):
        m = (p >= lo) & (p < hi) if hi < 1.0 else (p >= lo) & (p <= hi)
        cnt = int(m.sum())
        if cnt == 0:
            continue
        conf = float(p[m].mean())
        acc = float(y[m].mean())
        ece += (cnt / n) * abs(conf - acc)
    return float(ece)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--probe", type=Path, default=Path("benchmarks/sigma_gate_lsd/results_full/sigma_gate_lsd.pkl"))
    ap.add_argument("--csv", type=Path, default=Path("benchmarks/truthfulqa200/prompts.csv"))
    ap.add_argument("--model", default="gpt2", help="HF causal LM id (match probe for fair eval).")
    ap.add_argument("--max-new-tokens", type=int, default=100)
    ap.add_argument("--limit", type=int, default=None, help="Evaluate only first N rows.")
    args = ap.parse_args()

    repo = _repo_root()
    sys.path.insert(0, str(repo / "python"))

    import torch
    from transformers import AutoModelForCausalLM, AutoTokenizer

    from cos.sigma_gate import SigmaGate

    probe = (repo / args.probe).resolve() if not args.probe.is_absolute() else args.probe.resolve()
    csv_path = (repo / args.csv).resolve() if not args.csv.is_absolute() else args.csv.resolve()

    gate = SigmaGate(str(probe))
    try:
        tokenizer = AutoTokenizer.from_pretrained(args.model)
        if tokenizer.pad_token is None:
            tokenizer.pad_token = tokenizer.eos_token
        model = AutoModelForCausalLM.from_pretrained(args.model, output_hidden_states=True)
        model.eval()

        rows: List[Dict[str, str]] = []
        with csv_path.open(newline="", encoding="utf-8") as fp:
            for row in csv.DictReader(fp):
                rows.append(row)
        if args.limit is not None:
            rows = rows[: int(args.limit)]

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

            results.append(
                {
                    "id": row.get("id", f"tqa-{i:04d}"),
                    "sigma": float(sigma),
                    "decision": decision,
                    "correct": correct,
                    "response": response[:500],
                    "reference": reference[:500],
                }
            )
            mark = "ok" if correct else "x"
            print(f"[{len(results)}/{len(rows)}] sigma={sigma:.3f} {decision} {mark}")

        sigmas = np.array([r["sigma"] for r in results], dtype=np.float64)
        wrong = np.array([0 if r["correct"] else 1 for r in results], dtype=np.int64)
        decisions = [r["decision"] for r in results]

        if len(np.unique(wrong)) < 2:
            auroc = float("nan")
        else:
            auroc = float(roc_auc_score(wrong, sigmas))

        accepted = [r for r in results if r["decision"] == "ACCEPT"]
        acc_accepted = (
            sum(1 for r in accepted if r["correct"]) / len(accepted) if accepted else float("nan")
        )
        abstain_rate = sum(1 for d in decisions if d == "ABSTAIN") / len(decisions) if decisions else 0.0
        wrong_confident = sum(1 for r in results if r["sigma"] < gate.tau_accept and not r["correct"])
        coverage = sum(1 for d in decisions if d != "ABSTAIN") / len(decisions) if decisions else 0.0

        manifest_cv = float("nan")
        try:
            import pickle

            bundle = pickle.loads(probe.read_bytes())
            manifest_cv = float(bundle["manifest"].get("cv_auroc_mean", float("nan")))
        except Exception:
            pass

        ece = _ece_binary(wrong, sigmas) if len(results) else float("nan")

        summary = {
            "model": args.model,
            "n_prompts": len(results),
            "auroc_wrong_vs_sigma": auroc,
            "accuracy_accepted": acc_accepted if not math.isnan(acc_accepted) else None,
            "abstain_rate": float(abstain_rate),
            "wrong_confident_accept": int(wrong_confident),
            "coverage_non_abstain": float(coverage),
            "ece_wrong_vs_sigma_coarse": ece,
            "method": "sigma_gate_v4_lsd_contrastive",
            "cv_auroc_training_manifest": manifest_cv,
            "tau_accept": gate.tau_accept,
            "tau_abstain": gate.tau_abstain,
        }

        out_dir = repo / "benchmarks" / "sigma_gate_eval" / "results"
        out_dir.mkdir(parents=True, exist_ok=True)
        (out_dir / "eval_summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
        with (out_dir / "eval_detail.jsonl").open("w", encoding="utf-8") as jf:
            for r in results:
                jf.write(json.dumps(r) + "\n")

        print(f"\n{'=' * 50}")
        print("  sigma-gate v4 evaluation")
        print(f"{'=' * 50}")
        print(f"  AUROC (wrong vs sigma): {auroc:.4f}")
        print(
            f"  Accuracy @ accepted:    {acc_accepted:.4f}"
            if not math.isnan(acc_accepted)
            else "  Accuracy @ accepted:    n/a"
        )
        print(f"  Abstention rate:        {abstain_rate:.2%}")
        print(f"  Wrong + ACCEPT:         {wrong_confident}")
        print(f"  Coverage:               {coverage:.2%}")
        print(f"  ECE (coarse):           {ece:.4f}")
        print(f"{'=' * 50}")
    finally:
        gate.close()


if __name__ == "__main__":
    main()
