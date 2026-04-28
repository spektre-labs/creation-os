#!/usr/bin/env python3
"""
Evaluate sigma-gate v4 on HOLDOUT prompts (CSV not used during adapt_lsd training).

Default labels: **cosine similarity** (MiniLM-L6-v2) vs. ``best_answer`` — more robust
than substring match for verbose greedy generations. Optional ``--labeling substring``
for legacy comparison only.

AUROC: class 1 = incorrect (wrong) vs. sigma (hallucination score).

Requires: benchmarks/sigma_gate_lsd/.venv + PYTHONPATH=python; semantic mode needs
``pip install sentence-transformers`` (see run_holdout_pipeline.sh).
"""
from __future__ import annotations

import argparse
import csv
import json
import math
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


def _eval_path() -> Path:
    return Path(__file__).resolve().parent


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--probe",
        type=Path,
        default=Path("benchmarks/sigma_gate_lsd/results_holdout/sigma_gate_lsd.pkl"),
    )
    ap.add_argument(
        "--holdout-csv",
        type=Path,
        default=Path("benchmarks/sigma_gate_lsd/splits/holdout.csv"),
    )
    ap.add_argument("--generator-model", default="gpt2", help="HF causal LM for greedy completions.")
    ap.add_argument("--max-new-tokens", type=int, default=100)
    ap.add_argument(
        "--labeling",
        choices=("semantic", "substring"),
        default="semantic",
        help="correctness heuristic for greedy outputs (default: semantic cosine).",
    )
    ap.add_argument(
        "--semantic-threshold",
        type=float,
        default=0.45,
        help="Cosine threshold when --labeling semantic (higher = stricter).",
    )
    ap.add_argument(
        "--no-prism",
        action="store_true",
        help="Use raw CSV question for generation/scoring (default: PRISM-style suffix).",
    )
    args = ap.parse_args()

    repo = _repo_root()
    sys.path.insert(0, str(repo / "python"))
    sys.path.insert(0, str(_eval_path()))

    import torch
    from transformers import AutoModelForCausalLM, AutoTokenizer

    from cos.sigma_gate import SigmaGate

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

    if not probe.is_file():
        print(f"Missing probe: {probe}", file=sys.stderr)
        sys.exit(2)
    if not csv_path.is_file():
        print(f"Missing holdout CSV: {csv_path}", file=sys.stderr)
        sys.exit(2)

    training_cv = float("nan")
    try:
        bundle = pickle.loads(probe.read_bytes())
        training_cv = float(bundle["manifest"].get("cv_auroc_mean", float("nan")))
    except Exception:
        pass

    rows: List[Dict[str, str]] = []
    with csv_path.open(newline="", encoding="utf-8") as fp:
        for row in csv.DictReader(fp):
            rows.append(row)

    gate = SigmaGate(str(probe))
    try:
        tokenizer = AutoTokenizer.from_pretrained(args.generator_model)
        if tokenizer.pad_token is None:
            tokenizer.pad_token = tokenizer.eos_token
        model = AutoModelForCausalLM.from_pretrained(
            args.generator_model,
            output_hidden_states=True,
        )
        model.eval()

        results: List[Dict[str, Any]] = []
        for i, row in enumerate(rows):
            question = (row.get("question") or "").strip()
            reference = (row.get("best_answer") or "").strip()
            if not question:
                continue
            prompt_for_model = question if args.no_prism else prism_wrap_question(question)
            inputs = tokenizer(prompt_for_model, return_tensors="pt")
            dev = next(model.parameters()).device
            inputs = {k: v.to(dev) for k, v in inputs.items()}
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
            sigma, decision = gate(model, tokenizer, prompt_for_model, response)
            sim = None
            if args.labeling == "semantic":
                correct, sim = is_correct_semantic(
                    response, reference, threshold=float(args.semantic_threshold)
                )
            else:
                ref_l = reference.lower().strip()
                resp_l = response.lower().strip()
                correct = bool(ref_l) and (ref_l in resp_l or resp_l in ref_l)
            wrong = 0 if correct else 1

            results.append(
                {
                    "id": row.get("id", f"holdout-{i:04d}"),
                    "sigma": float(sigma),
                    "decision": decision,
                    "correct": correct,
                    "wrong": wrong,
                    "label_sim": sim,
                }
            )
            mark = "ok" if correct else "x"
            print(f"[{len(results)}/{len(rows)}] sigma={sigma:.3f} {decision} {mark}")

        sigmas = np.array([r["sigma"] for r in results], dtype=np.float64)
        labels = np.array([r["wrong"] for r in results], dtype=np.int64)

        if len(np.unique(labels)) < 2:
            auroc = float("nan")
            pr_auc = float("nan")
        else:
            auroc = float(roc_auc_score(labels, sigmas))
            pr_auc = float(average_precision_score(labels, sigmas))

        accepted = [r for r in results if r["decision"] == "ACCEPT"]
        acc_accepted = (
            sum(1 for r in accepted if r["correct"]) / len(accepted) if accepted else float("nan")
        )
        abstain_rate = sum(1 for r in results if r["decision"] == "ABSTAIN") / len(results) if results else 0.0
        wrong_confident = sum(1 for r in results if r["sigma"] < gate.tau_accept and not r["correct"])
        coverage = 1.0 - abstain_rate

        correct_sigmas = sigmas[labels == 0]
        wrong_sigmas = sigmas[labels == 1]
        sigma_gap = (
            float(np.mean(wrong_sigmas) - np.mean(correct_sigmas))
            if len(correct_sigmas) > 0 and len(wrong_sigmas) > 0
            else float("nan")
        )

        summary = {
            "eval_type": "HOLDOUT",
            "generator_model": args.generator_model,
            "n_holdout": len(results),
            "n_correct": int((labels == 0).sum()),
            "n_wrong": int((labels == 1).sum()),
            "auroc_wrong_vs_sigma": auroc,
            "pr_auc_average_precision": pr_auc,
            "accuracy_accepted": None if math.isnan(acc_accepted) else float(acc_accepted),
            "abstain_rate": float(abstain_rate),
            "wrong_confident_accept": int(wrong_confident),
            "coverage_non_abstain": float(coverage),
            "sigma_gap_mean_wrong_minus_correct": sigma_gap,
            "mean_sigma_correct": float(np.mean(correct_sigmas)) if len(correct_sigmas) else None,
            "mean_sigma_wrong": float(np.mean(wrong_sigmas)) if len(wrong_sigmas) else None,
            "training_cv_auroc_manifest": training_cv,
            "probe_path": str(probe.relative_to(repo)) if str(probe).startswith(str(repo)) else str(probe),
            "holdout_csv": str(csv_path.relative_to(repo)) if str(csv_path).startswith(str(repo)) else str(csv_path),
            "labeling": (
                f"semantic_cosine_minilm_threshold_{args.semantic_threshold}"
                if args.labeling == "semantic"
                else "substring_vs_best_answer"
            ),
            "prism_prompt": not bool(args.no_prism),
        }

        out_dir = repo / "benchmarks" / "sigma_gate_eval" / "results_holdout"
        out_dir.mkdir(parents=True, exist_ok=True)
        (out_dir / "holdout_summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
        with (out_dir / "holdout_detail.jsonl").open("w", encoding="utf-8") as jf:
            for r in results:
                jf.write(json.dumps(r) + "\n")

        print(f"\n{'=' * 50}")
        print("  sigma-gate v4 HOLDOUT evaluation")
        print(f"{'=' * 50}")
        print(f"  HOLDOUT AUROC:        {auroc:.4f}")
        print(f"  PR-AUC (AP):          {pr_auc:.4f}")
        if not math.isnan(acc_accepted):
            print(f"  Accuracy @ accepted: {acc_accepted:.4f}")
        else:
            print("  Accuracy @ accepted: n/a")
        print(f"  Abstention rate:      {abstain_rate:.2%}")
        print(f"  Wrong + ACCEPT:       {wrong_confident}")
        print(f"  Coverage:             {coverage:.2%}")
        print(f"  Sigma gap:            {sigma_gap:.4f}")
        print(f"{'=' * 50}")
        if not math.isnan(auroc):
            if auroc >= 0.9:
                print("  Verdict: HOLDOUT AUROC >= 0.90 (strong; still review label noise).")
            elif auroc >= 0.85:
                print("  Verdict: HOLDOUT AUROC >= 0.85")
            elif auroc >= 0.75:
                print("  Verdict: HOLDOUT AUROC >= 0.75")
            else:
                print("  Verdict: HOLDOUT AUROC < 0.75 — investigate overfit / labels / shift.")
    finally:
        gate.close()


if __name__ == "__main__":
    main()
