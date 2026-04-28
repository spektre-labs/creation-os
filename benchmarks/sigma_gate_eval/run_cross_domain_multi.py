#!/usr/bin/env python3
"""
Multi-benchmark lab harness: **LSD** vs **SpectralSigma** vs **SigmaUnified** on
streaming HF tasks (TriviaQA, SciQ, NQ-open, one MMLU subject, HaluEval generative).

Requires: ``benchmarks/sigma_gate_lsd/.venv`` + ``pip install datasets sentence-transformers``.
Network on first dataset access.

AUROC: positive class = **incorrect** greedy completion (``wrong=1``) vs. score.
This matches ``run_multi_signal_eval.py`` / ``run_holdout_eval.py``.

**Not** a reproduction of LapEigvals / EnsemHalDet / MMLU harness numbers; do not
merge these lab AUROCs with TruthfulQA CV headlines (see ``docs/CLAIM_DISCIPLINE.md``).

TruthfulQA **holdout** matrix rows should be produced with
``run_multi_signal_eval.py`` and merged offline with this summary JSON.
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
    from sklearn.metrics import average_precision_score, roc_auc_score
except ImportError as e:
    print("sklearn required:", e, file=sys.stderr)
    sys.exit(2)


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


def _training_cv_from_probe(probe: Path) -> float:
    try:
        bundle = pickle.loads(probe.read_bytes())
        return float(bundle["manifest"].get("cv_auroc_mean", float("nan")))
    except Exception:
        return float("nan")


def _fmt_auroc(x: float) -> str:
    return f"{x:.4f}" if isinstance(x, (int, float)) and not math.isnan(float(x)) else "nan"


def _auroc_pr(labels: np.ndarray, scores: np.ndarray) -> Tuple[float, float]:
    if labels.size == 0 or len(np.unique(labels)) < 2:
        return float("nan"), float("nan")
    return float(roc_auc_score(labels, scores)), float(average_precision_score(labels, scores))


def _greedy_one(
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


def _score_triple(
    lsd: Any,
    spectral: Any,
    unified: Any,
    model: Any,
    tokenizer: Any,
    q_in: str,
    response: str,
    reference: str | None,
) -> Tuple[float, float, float]:
    sl, _ = lsd(model, tokenizer, q_in, response, reference=reference)
    ss, _ = spectral.compute_sigma(model, tokenizer, q_in, response)
    su, _d, _det = unified(model, tokenizer, q_in, response, reference=reference)
    return float(sl), float(ss), float(su)


def _run_task_rows(
    name: str,
    rows_out: List[Dict[str, Any]],
) -> Dict[str, Any]:
    if not rows_out:
        return {"task": name, "n": 0, "error": "no_rows"}
    labels = np.array([int(r["wrong"]) for r in rows_out], dtype=np.int64)
    s_l = np.array([r["sigma_lsd"] for r in rows_out], dtype=np.float64)
    s_s = np.array([r["sigma_spectral"] for r in rows_out], dtype=np.float64)
    s_u = np.array([r["sigma_unified"] for r in rows_out], dtype=np.float64)
    al, pl = _auroc_pr(labels, s_l)
    as_, ps = _auroc_pr(labels, s_s)
    au, pu = _auroc_pr(labels, s_u)
    return {
        "task": name,
        "n": len(rows_out),
        "auroc_wrong_vs_sigma_lsd": al,
        "pr_auc_lsd": pl,
        "auroc_wrong_vs_sigma_spectral": as_,
        "pr_auc_spectral": ps,
        "auroc_wrong_vs_sigma_unified": au,
        "pr_auc_unified": pu,
        "error": None,
    }


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--model", default="gpt2")
    ap.add_argument("--limit-per-task", type=int, default=100)
    ap.add_argument("--max-new-tokens", type=int, default=100)
    ap.add_argument("--semantic-threshold", type=float, default=0.45)
    ap.add_argument("--no-prism", action="store_true")
    ap.add_argument(
        "--tasks",
        default="triviaqa,sciq,nq_open,mmlu,halueval_generative",
        help="Comma-separated subset of: triviaqa,sciq,nq_open,mmlu,halueval_generative",
    )
    ap.add_argument(
        "--mmlu-subject",
        default="high_school_mathematics",
        help="``cais/mmlu`` config name (one subject only for streaming smoke).",
    )
    ap.add_argument(
        "--spectral-backend",
        choices=("normalized_symmetric", "causal_combinatorial_fast"),
        default="causal_combinatorial_fast",
        help="Default fast combinatorial-causal Laplacian (O(n) per head); "
        "use normalized_symmetric for the eigendecomposition path.",
    )
    args = ap.parse_args()

    repo = _repo_root()
    sys.path.insert(0, str(repo / "python"))
    sys.path.insert(0, str(_eval_dir()))

    try:
        import datasets  # noqa: F401
    except ImportError:
        print("Install datasets: pip install datasets", file=sys.stderr)
        sys.exit(3)

    from datasets import load_dataset

    from transformers import AutoModelForCausalLM, AutoTokenizer

    from cos.sigma_gate import SigmaGate
    from cos.sigma_spectral import SpectralSigma
    from cos.sigma_unified import SigmaUnified

    try:
        from semantic_labeling import (
            cosine_similarity,
            is_correct_semantic,
            is_correct_vs_any_reference,
            prism_wrap_question,
        )
    except ImportError as e:
        print("semantic_labeling import failed:", e, file=sys.stderr)
        sys.exit(3)

    use_prism = not bool(args.no_prism)
    tasks = [t.strip() for t in str(args.tasks).split(",") if t.strip()]
    limit = int(args.limit_per_task)

    probe = _pick_probe(repo)
    training_cv = _training_cv_from_probe(probe)

    spectral = SpectralSigma(k=10, laplacian_backend=str(args.spectral_backend))
    lsd_gate = SigmaGate(str(probe))
    unified = SigmaUnified(
        str(probe),
        spectral_k=10,
        spectral_laplacian_backend=str(args.spectral_backend),
    )

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

    all_detail: List[Dict[str, Any]] = []
    per_task: Dict[str, Any] = {}

    try:
        for task in tasks:
            rows: List[Dict[str, Any]] = []
            err = ""

            if task == "triviaqa":
                try:
                    ds = load_dataset(
                        "trivia_qa",
                        "unfiltered.nocontext",
                        split="validation",
                        streaming=True,
                    )
                except Exception as e:
                    err = f"trivia_load:{e}"
                    per_task[task] = {"task": task, "n": 0, "error": err}
                    continue
                for row in ds:
                    if len(rows) >= limit:
                        break
                    question = (row.get("question") or "").strip()
                    aliases = row.get("answer", {}).get("aliases") or []
                    if not question or not aliases:
                        continue
                    q_in = prism_wrap_question(question) if use_prism else question
                    response = _greedy_one(model, tok, q_in, max_new_tokens=args.max_new_tokens, device=device)
                    sl, ss, su = _score_triple(
                        lsd_gate, spectral, unified, model, tok, q_in, response, None
                    )
                    ok, _sim = is_correct_vs_any_reference(
                        response, [str(a) for a in aliases], threshold=float(args.semantic_threshold)
                    )
                    wrong = 0 if ok else 1
                    rows.append(
                        {
                            "task": "triviaqa",
                            "sigma_lsd": sl,
                            "sigma_spectral": ss,
                            "sigma_unified": su,
                            "wrong": wrong,
                        }
                    )
                    print(f"[triviaqa {len(rows)}/{limit}] LSD={sl:.3f} SPEC={ss:.3f} UNI={su:.3f} {'ok' if ok else 'x'}")

            elif task == "sciq":
                try:
                    ds = load_dataset("allenai/sciq", split="test", streaming=True)
                except Exception as e:
                    err = f"sciq_load:{e}"
                    per_task[task] = {"task": task, "n": 0, "error": err}
                    continue
                for row in ds:
                    if len(rows) >= limit:
                        break
                    question = (row.get("question") or "").strip()
                    gold = (row.get("correct_answer") or "").strip()
                    if not question or not gold:
                        continue
                    q_in = prism_wrap_question(question) if use_prism else question
                    response = _greedy_one(model, tok, q_in, max_new_tokens=args.max_new_tokens, device=device)
                    sl, ss, su = _score_triple(
                        lsd_gate, spectral, unified, model, tok, q_in, response, gold
                    )
                    ok, _ = is_correct_semantic(
                        response, gold, threshold=float(args.semantic_threshold)
                    )
                    wrong = 0 if ok else 1
                    rows.append(
                        {
                            "task": "sciq",
                            "sigma_lsd": sl,
                            "sigma_spectral": ss,
                            "sigma_unified": su,
                            "wrong": wrong,
                        }
                    )
                    print(f"[sciq {len(rows)}/{limit}] LSD={sl:.3f} SPEC={ss:.3f} UNI={su:.3f} {'ok' if ok else 'x'}")

            elif task == "nq_open":
                try:
                    ds = load_dataset("nq_open", split="validation", streaming=True)
                except Exception as e:
                    err = f"nq_load:{e}"
                    per_task[task] = {"task": task, "n": 0, "error": err}
                    continue
                for row in ds:
                    if len(rows) >= limit:
                        break
                    question = (row.get("question") or "").strip()
                    answers = row.get("answer") or []
                    refs = [str(a) for a in answers if str(a).strip()]
                    if not question or not refs:
                        continue
                    q_in = prism_wrap_question(question) if use_prism else question
                    response = _greedy_one(model, tok, q_in, max_new_tokens=args.max_new_tokens, device=device)
                    sl, ss, su = _score_triple(
                        lsd_gate, spectral, unified, model, tok, q_in, response, refs[0]
                    )
                    ok, _ = is_correct_vs_any_reference(
                        response, refs, threshold=float(args.semantic_threshold)
                    )
                    wrong = 0 if ok else 1
                    rows.append(
                        {
                            "task": "nq_open",
                            "sigma_lsd": sl,
                            "sigma_spectral": ss,
                            "sigma_unified": su,
                            "wrong": wrong,
                        }
                    )
                    print(f"[nq_open {len(rows)}/{limit}] LSD={sl:.3f} SPEC={ss:.3f} UNI={su:.3f} {'ok' if ok else 'x'}")

            elif task == "mmlu":
                subj = str(args.mmlu_subject)
                try:
                    ds = load_dataset("cais/mmlu", subj, split="test", streaming=True)
                except Exception as e:
                    err = f"mmlu_load:{e}"
                    per_task[task] = {"task": task, "n": 0, "error": err}
                    continue
                for row in ds:
                    if len(rows) >= limit:
                        break
                    qtext = (row.get("question") or "").strip()
                    choices = row.get("choices") or []
                    ans_idx = row.get("answer")
                    if not qtext or not choices or ans_idx is None:
                        continue
                    try:
                        ai = int(ans_idx)
                    except (TypeError, ValueError):
                        continue
                    if ai < 0 or ai >= len(choices):
                        continue
                    gold = str(choices[ai]).strip()
                    letters = "ABCD"
                    opt_lines = "\n".join(f"{letters[i]}. {choices[i]}" for i in range(len(choices)))
                    question = f"{qtext}\n{opt_lines}"
                    q_in = prism_wrap_question(question) if use_prism else question
                    response = _greedy_one(model, tok, q_in, max_new_tokens=args.max_new_tokens, device=device)
                    sl, ss, su = _score_triple(
                        lsd_gate, spectral, unified, model, tok, q_in, response, gold
                    )
                    ok, _ = is_correct_semantic(
                        response, gold, threshold=float(args.semantic_threshold)
                    )
                    wrong = 0 if ok else 1
                    rows.append(
                        {
                            "task": "mmlu",
                            "mmlu_subject": subj,
                            "sigma_lsd": sl,
                            "sigma_spectral": ss,
                            "sigma_unified": su,
                            "wrong": wrong,
                        }
                    )
                    print(f"[mmlu {len(rows)}/{limit}] LSD={sl:.3f} SPEC={ss:.3f} UNI={su:.3f} {'ok' if ok else 'x'}")

            elif task == "halueval_generative":
                try:
                    ds = load_dataset("pminervini/HaluEval", "qa_samples", split="data", streaming=True)
                except Exception as e:
                    err = f"halueval_load:{e}"
                    per_task[task] = {"task": task, "n": 0, "error": err}
                    continue
                for row in ds:
                    if len(rows) >= limit:
                        break
                    question = (row.get("question") or "").strip()
                    ref_answer = (row.get("answer") or "").strip()
                    hal_raw = (row.get("hallucination") or "").strip().lower()
                    if not question or not ref_answer or hal_raw not in ("yes", "no"):
                        continue
                    q_in = prism_wrap_question(question) if use_prism else question
                    response = _greedy_one(model, tok, q_in, max_new_tokens=args.max_new_tokens, device=device)
                    sl, ss, su = _score_triple(
                        lsd_gate, spectral, unified, model, tok, q_in, response, ref_answer
                    )
                    sim = cosine_similarity(response, ref_answer)
                    if hal_raw == "no":
                        ok = sim > float(args.semantic_threshold)
                    else:
                        ok = sim <= float(args.semantic_threshold)
                    wrong = 0 if ok else 1
                    rows.append(
                        {
                            "task": "halueval_generative",
                            "sigma_lsd": sl,
                            "sigma_spectral": ss,
                            "sigma_unified": su,
                            "wrong": wrong,
                        }
                    )
                    print(
                        f"[halueval_gen {len(rows)}/{limit}] LSD={sl:.3f} SPEC={ss:.3f} "
                        f"UNI={su:.3f} halu={hal_raw} {'ok' if ok else 'x'}"
                    )
            else:
                per_task[task] = {"task": task, "n": 0, "error": f"unknown_task:{task}"}
                continue

            all_detail.extend(rows)
            per_task[task] = _run_task_rows(task, rows)

        summary = {
            "eval_type": "CROSS_DOMAIN_MULTI_SIGNAL",
            "generator_model": args.model,
            "prism_prompt": use_prism,
            "spectral_laplacian_backend": str(args.spectral_backend),
            "mmlu_subject": str(args.mmlu_subject),
            "limit_per_task": limit,
            "training_cv_auroc_manifest": training_cv,
            "probe_path": str(probe),
            "tasks": per_task,
            "truthfulqa_holdout_note": "Run benchmarks/sigma_gate_eval/run_multi_signal_eval.py for holdout LSD/spectral/unified AUROCs.",
            "disclaimer": "Do not claim universal σ-gate from a single-script sweep; validate per domain and label noise.",
        }

        out_dir = repo / "benchmarks" / "sigma_gate_eval" / "results_cross_domain_multi"
        out_dir.mkdir(parents=True, exist_ok=True)
        (out_dir / "cross_domain_multi_summary.json").write_text(
            json.dumps(summary, indent=2),
            encoding="utf-8",
        )
        with (out_dir / "cross_domain_multi_detail.jsonl").open("w", encoding="utf-8") as jf:
            for r in all_detail:
                jf.write(json.dumps(r) + "\n")

        print(f"\n{'=' * 60}")
        print("  Cross-domain multi-signal (lab harness)")
        print(f"{'=' * 60}")
        for tname, block in per_task.items():
            print(
                f"  {tname:22s} n={block.get('n', 0):4d}  "
                f"LSD={_fmt_auroc(float(block.get('auroc_wrong_vs_sigma_lsd', float('nan'))))}  "
                f"SPEC={_fmt_auroc(float(block.get('auroc_wrong_vs_sigma_spectral', float('nan'))))}  "
                f"UNI={_fmt_auroc(float(block.get('auroc_wrong_vs_sigma_unified', float('nan'))))}"
                f"  err={block.get('error')!r}"
            )
        print(f"{'=' * 60}")
    finally:
        lsd_gate.close()
        unified.close()


if __name__ == "__main__":
    main()
