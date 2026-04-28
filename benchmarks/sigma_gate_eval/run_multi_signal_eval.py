#!/usr/bin/env python3
"""
Compare LSD ``SigmaGate``, ``SpectralSigma`` heuristic, and ``SigmaUnified`` fusion
on the same holdout protocol as ``run_holdout_eval.py``.

AUROC: positive class = **incorrect** greedy completion (``wrong=1``) vs. score
(higher score = more hallucination-oriented). This matches ``run_holdout_eval.py``.

Results: ``benchmarks/sigma_gate_eval/results_multi_signal/multi_signal_summary.json``.
Does **not** overwrite ``results_holdout/holdout_summary.json``.
"""
from __future__ import annotations

import argparse
import csv
import json
import pickle
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
    ap.add_argument("--limit", type=int, default=0, help="If >0, only first N CSV rows (smoke).")
    ap.add_argument(
        "--spectral-backend",
        choices=("normalized_symmetric", "causal_combinatorial_fast"),
        default="normalized_symmetric",
        help="Laplacian mode for SpectralSigma / SigmaUnified (see python/cos/sigma_spectral.py).",
    )
    args = ap.parse_args()

    repo = _repo_root()
    sys.path.insert(0, str(repo / "python"))
    sys.path.insert(0, str(_eval_dir()))

    import torch
    from transformers import AutoModelForCausalLM, AutoTokenizer

    from cos.sigma_gate import SigmaGate
    from cos.sigma_spectral import SpectralSigma
    from cos.sigma_unified import SigmaUnified

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
    if int(args.limit) > 0:
        rows = rows[: int(args.limit)]

    lsd_gate = SigmaGate(str(probe))
    spectral = SpectralSigma(k=10, laplacian_backend=str(args.spectral_backend))
    unified = SigmaUnified(str(probe), spectral_k=10, spectral_laplacian_backend=str(args.spectral_backend))

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

            sigma_lsd, _dec = lsd_gate(model, tok, prompt_for_model, response, reference=reference)
            sigma_spec, _feat = spectral.compute_sigma(model, tok, prompt_for_model, response)
            sigma_uni, _dec_u, _details = unified(model, tok, prompt_for_model, response, reference=reference)

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
                    "sigma_lsd": float(sigma_lsd),
                    "sigma_spectral": float(sigma_spec),
                    "sigma_unified": float(sigma_uni),
                    "wrong": wrong,
                    "label_sim": sim,
                }
            )
            mark = "ok" if correct else "x"
            print(f"[{len(results)}/{len(rows)}] LSD={sigma_lsd:.3f} SPEC={sigma_spec:.3f} UNI={sigma_uni:.3f} {mark}")

        labels = np.array([r["wrong"] for r in results], dtype=np.int64)
        s_lsd = np.array([r["sigma_lsd"] for r in results], dtype=np.float64)
        s_spec = np.array([r["sigma_spectral"] for r in results], dtype=np.float64)
        s_uni = np.array([r["sigma_unified"] for r in results], dtype=np.float64)

        def _auroc_pr(y: np.ndarray, s: np.ndarray) -> Tuple[float, float]:
            if len(np.unique(y)) < 2:
                return float("nan"), float("nan")
            return float(roc_auc_score(y, s)), float(average_precision_score(y, s))

        auroc_lsd, pr_lsd = _auroc_pr(labels, s_lsd)
        auroc_spec, pr_spec = _auroc_pr(labels, s_spec)
        auroc_uni, pr_uni = _auroc_pr(labels, s_uni)

        summary = {
            "eval_type": "MULTI_SIGNAL_HOLDOUT",
            "generator_model": args.generator_model,
            "n": len(results),
            "n_wrong": int((labels == 1).sum()),
            "n_correct": int((labels == 0).sum()),
            "auroc_wrong_vs_sigma_lsd": auroc_lsd,
            "pr_auc_lsd": pr_lsd,
            "auroc_wrong_vs_sigma_spectral": auroc_spec,
            "pr_auc_spectral": pr_spec,
            "auroc_wrong_vs_sigma_unified": auroc_uni,
            "pr_auc_unified": pr_uni,
            "training_cv_auroc_manifest": training_cv,
            "probe_path": str(probe),
            "holdout_csv": str(csv_path),
            "labeling": str(args.labeling),
            "spectral_laplacian_backend": str(args.spectral_backend),
            "disclaimer": "Fusion AUROC is not claimed to exceed LSD until validated on your split.",
        }

        out_dir = repo / "benchmarks" / "sigma_gate_eval" / "results_multi_signal"
        out_dir.mkdir(parents=True, exist_ok=True)
        (out_dir / "multi_signal_summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
        with (out_dir / "multi_signal_detail.jsonl").open("w", encoding="utf-8") as jf:
            for r in results:
                jf.write(json.dumps(r) + "\n")

        print(f"\n  LSD AUROC:      {auroc_lsd:.4f}  PR-AUC: {pr_lsd:.4f}")
        print(f"  Spectral AUROC: {auroc_spec:.4f}  PR-AUC: {pr_spec:.4f}")
        print(f"  Unified AUROC:  {auroc_uni:.4f}  PR-AUC: {pr_uni:.4f}")
    finally:
        lsd_gate.close()
        unified.close()


if __name__ == "__main__":
    main()
