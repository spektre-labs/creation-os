#!/usr/bin/env python3
"""
Cross-domain / cross-task smoke: TruthfulQA-trained LSD probe (no retraining)
on TriviaQA (generated answers) and HaluEval **generative** QA (`qa_samples`).

Default: **PRISM-style** prompt + greedy GPT-2 generation, then σ on the
**generated** completion. TriviaQA: semantic match to any alias. HaluEval:
``qa_samples`` rows — label ``wrong`` uses dataset ``hallucination`` flag vs
cosine alignment to the provided ``answer`` string (not oracle pre-scored
hidden states on gold strings).

Requires: ``pip install datasets sentence-transformers`` in the LSD venv. Network at first run.

Evidence: AUROCs are **not** comparable to TruthfulQA CV (different labels and
generators). This script is an **empirical smoke harness**, not a reproduction of
external papers' metrics.
"""
from __future__ import annotations

import argparse
import json
import math
import pickle
import sys
from pathlib import Path
from typing import Any, Dict, List, Tuple

import numpy as np

try:
    from sklearn.metrics import roc_auc_score
except ImportError as e:
    print("sklearn required:", e, file=sys.stderr)
    sys.exit(2)


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


def _training_cv_from_probe(probe: Path) -> float:
    try:
        bundle = pickle.loads(probe.read_bytes())
        return float(bundle["manifest"].get("cv_auroc_mean", float("nan")))
    except Exception:
        return float("nan")


def _fmt_auroc(x: float) -> str:
    return f"{x:.4f}" if isinstance(x, (int, float)) and not math.isnan(float(x)) else "nan"


def eval_triviaqa(
    gate: Any,
    model: Any,
    tokenizer: Any,
    limit: int,
    *,
    use_prism: bool,
    trivia_labeling: str,
    semantic_threshold: float,
) -> Tuple[float, List[Dict[str, Any]], str]:
    from datasets import load_dataset

    sys.path.insert(0, str(Path(__file__).resolve().parent))
    if use_prism or trivia_labeling == "semantic":
        try:
            from semantic_labeling import prism_wrap_question

            if trivia_labeling == "semantic":
                from semantic_labeling import is_correct_vs_any_reference
        except ImportError as e:
            return float("nan"), [], f"semantic_labeling_import:{e}"
    else:

        def prism_wrap_question(q: str) -> str:  # type: ignore[misc]
            return (q or "").strip()

    err = ""
    results: List[Dict[str, Any]] = []
    try:
        ds = load_dataset(
            "trivia_qa",
            "unfiltered.nocontext",
            split="validation",
            streaming=True,
        )
    except Exception as e:
        return float("nan"), [], f"trivia_load:{e}"

    for row in ds:
        if len(results) >= limit:
            break
        question = (row.get("question") or "").strip()
        aliases = row.get("answer", {}).get("aliases") or []
        if not question or not aliases:
            continue
        q_in = prism_wrap_question(question) if use_prism else question
        inputs = tokenizer(q_in, return_tensors="pt")
        dev = next(model.parameters()).device
        inputs = {k: v.to(dev) for k, v in inputs.items()}
        import torch

        with torch.no_grad():
            out = model.generate(
                **inputs,
                max_new_tokens=100,
                do_sample=False,
                pad_token_id=tokenizer.pad_token_id,
            )
        response = tokenizer.decode(
            out[0][inputs["input_ids"].shape[1] :],
            skip_special_tokens=True,
        ).strip()
        sigma, decision = gate(model, tokenizer, q_in, response)
        if trivia_labeling == "semantic":
            correct, _sim = is_correct_vs_any_reference(
                response, [str(a) for a in aliases], threshold=float(semantic_threshold)
            )
        else:
            resp_l = response.lower()
            correct = any(str(a).lower() in resp_l for a in aliases)
        wrong = 0 if correct else 1
        results.append(
            {
                "task": "triviaqa",
                "sigma": float(sigma),
                "decision": decision,
                "correct": correct,
                "wrong": wrong,
            }
        )
        mark = "ok" if correct else "x"
        print(f"[TriviaQA {len(results)}/{limit}] sigma={sigma:.3f} {decision} {mark}")

    if not results or len(np.unique([r["wrong"] for r in results])) < 2:
        return float("nan"), results, err or "trivia_single_class"
    sigmas = np.array([r["sigma"] for r in results], dtype=np.float64)
    labels = np.array([r["wrong"] for r in results], dtype=np.int64)
    return float(roc_auc_score(labels, sigmas)), results, err


def eval_halueval_generative_qa_samples(
    gate: Any,
    model: Any,
    tokenizer: Any,
    limit: int,
    *,
    use_prism: bool,
    semantic_threshold: float,
) -> Tuple[float, List[Dict[str, Any]], str]:
    """HaluEval ``qa_samples``: greedy generation + σ on generated text; labels from HF fields."""
    from datasets import load_dataset

    sys.path.insert(0, str(Path(__file__).resolve().parent))
    try:
        from semantic_labeling import cosine_similarity, prism_wrap_question
    except ImportError as e:
        return float("nan"), [], f"semantic_labeling_import:{e}"

    try:
        ds = load_dataset(
            "pminervini/HaluEval",
            "qa_samples",
            split="data",
            streaming=True,
        )
    except Exception as e:
        return float("nan"), [], f"halueval_load:{e}"

    results: List[Dict[str, Any]] = []
    for row in ds:
        if len(results) >= limit:
            break
        question = (row.get("question") or "").strip()
        ref_answer = (row.get("answer") or "").strip()
        hal_raw = (row.get("hallucination") or "").strip().lower()
        if not question or not ref_answer or hal_raw not in ("yes", "no"):
            continue
        q_in = prism_wrap_question(question) if use_prism else question
        inputs = tokenizer(q_in, return_tensors="pt")
        dev = next(model.parameters()).device
        inputs = {k: v.to(dev) for k, v in inputs.items()}
        import torch

        with torch.no_grad():
            out = model.generate(
                **inputs,
                max_new_tokens=100,
                do_sample=False,
                pad_token_id=tokenizer.pad_token_id,
            )
        response = tokenizer.decode(
            out[0][inputs["input_ids"].shape[1] :],
            skip_special_tokens=True,
        ).strip()
        sigma, decision = gate(model, tokenizer, q_in, response)
        sim = cosine_similarity(response, ref_answer)
        # Dataset "answer" is factual when hallucination=no; when yes, it is a false completion.
        if hal_raw == "no":
            correct = sim > float(semantic_threshold)
        else:
            correct = sim <= float(semantic_threshold)
        wrong = 0 if correct else 1
        results.append(
            {
                "task": "halueval_generative",
                "sigma": float(sigma),
                "decision": decision,
                "correct": correct,
                "wrong": wrong,
                "sim": float(sim),
                "dataset_hallucination": hal_raw,
            }
        )
        mark = "ok" if correct else "x"
        print(
            f"[HaluEval gen {len(results)}/{limit}] sigma={sigma:.3f} {decision} "
            f"halu={hal_raw} sim={sim:.3f} {mark}"
        )

    if not results or len(np.unique([r["wrong"] for r in results])) < 2:
        return float("nan"), results, "halueval_gen_single_class"
    sigmas = np.array([r["sigma"] for r in results], dtype=np.float64)
    labels = np.array([r["wrong"] for r in results], dtype=np.int64)
    return float(roc_auc_score(labels, sigmas)), results, ""


def eval_halueval_qa_oracle_pairs(
    gate: Any,
    model: Any,
    tokenizer: Any,
    limit: int,
    *,
    use_prism: bool,
) -> Tuple[float, List[Dict[str, Any]], str]:
    from datasets import load_dataset

    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from semantic_labeling import prism_wrap_question

    try:
        ds = load_dataset("pminervini/HaluEval", "qa", split="data", streaming=True)
    except Exception as e:
        return float("nan"), [], f"halueval_load:{e}"

    results: List[Dict[str, Any]] = []
    pairs_seen = 0
    for row in ds:
        if pairs_seen >= limit:
            break
        question = (row.get("question") or "").strip()
        hal = (row.get("hallucinated_answer") or "").strip()
        right = (row.get("right_answer") or "").strip()
        if not question or not hal or not right:
            continue
        q_in = prism_wrap_question(question) if use_prism else question
        sigma_hal, dec_hal = gate(model, tokenizer, q_in, hal)
        sigma_right, dec_right = gate(model, tokenizer, q_in, right)
        results.append(
            {
                "task": "halueval",
                "sigma": float(sigma_hal),
                "decision": dec_hal,
                "correct": False,
                "wrong": 1,
                "pair": pairs_seen,
                "arm": "hallucinated",
            }
        )
        results.append(
            {
                "task": "halueval",
                "sigma": float(sigma_right),
                "decision": dec_right,
                "correct": True,
                "wrong": 0,
                "pair": pairs_seen,
                "arm": "right",
            }
        )
        pairs_seen += 1
        print(
            f"[HaluEval pair {pairs_seen}/{limit}] "
            f"halluc_sigma={sigma_hal:.3f} right_sigma={sigma_right:.3f}"
        )

    if not results or len(np.unique([r["wrong"] for r in results])) < 2:
        return float("nan"), results, "halueval_single_class"
    sigmas = np.array([r["sigma"] for r in results], dtype=np.float64)
    labels = np.array([r["wrong"] for r in results], dtype=np.int64)
    return float(roc_auc_score(labels, sigmas)), results, ""


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--trivia-limit", type=int, default=100)
    ap.add_argument("--halu-limit", type=int, default=100)
    ap.add_argument("--model", default="gpt2")
    ap.add_argument(
        "--trivia-labeling",
        choices=("semantic", "substring"),
        default="semantic",
        help="How to label TriviaQA greedy generations vs aliases.",
    )
    ap.add_argument("--semantic-threshold", type=float, default=0.45)
    ap.add_argument(
        "--no-prism",
        action="store_true",
        help="Disable PRISM-style prompt suffix for generation and scoring.",
    )
    ap.add_argument(
        "--halueval-mode",
        choices=("generative", "oracle"),
        default="generative",
        help="generative: qa_samples + greedy GPT-2 + σ on generation; oracle: legacy qa pair scoring.",
    )
    args = ap.parse_args()

    repo = _repo_root()
    sys.path.insert(0, str(repo / "python"))

    try:
        import datasets  # noqa: F401
    except ImportError:
        print("Install datasets: pip install datasets", file=sys.stderr)
        sys.exit(3)

    import torch
    from transformers import AutoModelForCausalLM, AutoTokenizer

    from cos.sigma_gate import SigmaGate

    use_prism = not bool(args.no_prism)
    probe = _pick_probe(repo)
    gate = SigmaGate(str(probe))
    try:
        tokenizer = AutoTokenizer.from_pretrained(args.model)
        if tokenizer.pad_token is None:
            tokenizer.pad_token = tokenizer.eos_token
        model = AutoModelForCausalLM.from_pretrained(
            args.model,
            output_hidden_states=True,
        )
        model.eval()

        print(
            f"=== TriviaQA (PRISM={use_prism}, labeling={args.trivia_labeling}) ==="
        )
        trivia_auroc, trivia_res, trivia_err = eval_triviaqa(
            gate,
            model,
            tokenizer,
            int(args.trivia_limit),
            use_prism=use_prism,
            trivia_labeling=str(args.trivia_labeling),
            semantic_threshold=float(args.semantic_threshold),
        )
        print(f"TriviaQA AUROC (wrong vs sigma): {_fmt_auroc(trivia_auroc)} err={trivia_err!r}")

        if str(args.halueval_mode) == "oracle":
            print("\n=== HaluEval QA (oracle pairs, legacy) ===")
            halu_auroc, halu_res, halu_err = eval_halueval_qa_oracle_pairs(
                gate,
                model,
                tokenizer,
                int(args.halu_limit),
                use_prism=use_prism,
            )
        else:
            print("\n=== HaluEval (generative qa_samples) ===")
            halu_auroc, halu_res, halu_err = eval_halueval_generative_qa_samples(
                gate,
                model,
                tokenizer,
                int(args.halu_limit),
                use_prism=use_prism,
                semantic_threshold=float(args.semantic_threshold),
            )
        print(f"HaluEval AUROC (wrong vs sigma): {_fmt_auroc(halu_auroc)} err={halu_err!r}")

        training_cv = _training_cv_from_probe(probe)
        halu_n_pairs = int(len(halu_res) // 2) if str(args.halueval_mode) == "oracle" else 0
        halu_n_gen = len(halu_res) if str(args.halueval_mode) == "generative" else 0
        summary = {
            "probe_path": str(probe.relative_to(repo)) if str(probe).startswith(str(repo)) else str(probe),
            "generator_model": args.model,
            "probe_trained_on": "TruthfulQA split (see results_holdout or results_full manifest)",
            "training_cv_auroc_manifest": training_cv,
            "prism_prompt": use_prism,
            "trivia_labeling": str(args.trivia_labeling),
            "trivia_semantic_threshold": float(args.semantic_threshold),
            "triviaqa_auroc_wrong_vs_sigma": trivia_auroc,
            "triviaqa_n": len(trivia_res),
            "triviaqa_error": trivia_err or None,
            "halueval_mode": str(args.halueval_mode),
            "halueval_auroc_wrong_vs_sigma": halu_auroc,
            "halueval_n_pairs_oracle": halu_n_pairs,
            "halueval_n_generative": halu_n_gen,
            "halueval_n_rows": len(halu_res),
            "halueval_error": halu_err or None,
            "generalization_threshold_0p70": {
                "triviaqa_pass": bool(trivia_auroc >= 0.70) if not math.isnan(trivia_auroc) else False,
                "halueval_pass": bool(halu_auroc >= 0.70) if not math.isnan(halu_auroc) else False,
            },
        }
        summary["generalization_both_pass"] = bool(
            summary["generalization_threshold_0p70"]["triviaqa_pass"]
            and summary["generalization_threshold_0p70"]["halueval_pass"]
        )

        out_dir = repo / "benchmarks" / "sigma_gate_eval" / "results_cross_domain"
        out_dir.mkdir(parents=True, exist_ok=True)
        (out_dir / "cross_domain_summary.json").write_text(
            json.dumps(summary, indent=2),
            encoding="utf-8",
        )
        with (out_dir / "cross_domain_detail.jsonl").open("w", encoding="utf-8") as jf:
            for r in trivia_res + halu_res:
                jf.write(json.dumps(r) + "\n")

        print(f"\n{'=' * 50}")
        print("  Cross-domain summary (smoke harness)")
        print(f"{'=' * 50}")
        print(f"  TriviaQA AUROC:  {_fmt_auroc(trivia_auroc)}  (n={len(trivia_res)})")
        print(f"  HaluEval AUROC: {_fmt_auroc(halu_auroc)}  (mode={args.halueval_mode}, n={len(halu_res)})")
        print(f"  Both >= 0.70:   {summary['generalization_both_pass']}")
        print(f"{'=' * 50}")
    finally:
        gate.close()


if __name__ == "__main__":
    main()
