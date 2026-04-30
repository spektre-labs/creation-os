#!/usr/bin/env python3
"""
Compare **LSD**, **HIDE** (upstream unbiased HSIC + KeyBERT when ``keybert`` is installed;
otherwise lab forward HSIC), **SpectralSigma**, and **SigmaUltimate** fusion
on TruthfulQA holdout, TriviaQA streaming, and HaluEval ``qa_samples`` (generative).

AUROC: ``wrong=1`` (incorrect) vs. score (higher = more hallucination-oriented).

Requires: ``benchmarks/sigma_gate_lsd/.venv`` + ``datasets`` + ``sentence-transformers``.
For the **official** HIDE path inside ``SigmaHIDE`` (``--hide-backend official`` or ``auto`` when
``keybert`` is installed), also ``pip install keybert``.
This harness is **not** a reproduction of HIDE paper numbers — validate before claims.
"""
from __future__ import annotations

import argparse
import csv
import json
import math
import pickle
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


def _pick_probe(repo: Path) -> Path:
    for rel in (
        "benchmarks/sigma_gate_lsd/results_holdout/sigma_gate_lsd.pkl",
        "benchmarks/sigma_gate_lsd/results_full/sigma_gate_lsd.pkl",
    ):
        p = repo / rel
        if p.is_file():
            return p
    raise FileNotFoundError("No sigma_gate_lsd.pkl")


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


def _score_row(
    scorers: Dict[str, Callable[..., float]],
    model: Any,
    tok: Any,
    q_in: str,
    response: str,
    reference: str | None,
) -> Dict[str, float]:
    out: Dict[str, float] = {}
    for name, fn in scorers.items():
        try:
            out[name] = float(fn(model, tok, q_in, response, reference))
        except Exception:
            out[name] = 0.5
    return out


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--model", default="gpt2")
    ap.add_argument("--holdout-limit", type=int, default=0, help="0 = all holdout rows")
    ap.add_argument("--trivia-limit", type=int, default=100)
    ap.add_argument("--halu-limit", type=int, default=100)
    ap.add_argument("--max-new-tokens", type=int, default=100)
    ap.add_argument("--semantic-threshold", type=float, default=0.45)
    ap.add_argument("--no-prism", action="store_true")
    ap.add_argument(
        "--spectral-backend",
        choices=("normalized_symmetric", "causal_combinatorial_fast"),
        default="normalized_symmetric",
    )
    ap.add_argument(
        "--hide-backend",
        choices=("auto", "official", "lab"),
        default="auto",
        help="HIDE channel: official upstream path needs keybert+torch; lab is forward HSIC only",
    )
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

    from cos.sigma_gate import SigmaGate
    from cos.sigma_hide import SigmaHIDE
    from cos.sigma_spectral import SpectralSigma
    from cos.sigma_ultimate import SigmaUltimate

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
    probe = _pick_probe(repo)
    device = None

    lsd_gate = SigmaGate(str(probe))
    hide = SigmaHIDE(backend=str(args.hide_backend))
    spectral = SpectralSigma(k=10, laplacian_backend=str(args.spectral_backend))
    ultimate = SigmaUltimate(
        str(probe),
        spectral_k=10,
        spectral_laplacian_backend=str(args.spectral_backend),
        hide_backend=str(args.hide_backend),
    )

    def _lsd_fn(m: Any, t: Any, q: str, r: str, reference: str | None) -> float:
        return lsd_gate(m, t, q, r, reference=reference)[0]

    def _hide_fn(m: Any, t: Any, q: str, r: str, reference: str | None) -> float:
        return hide.compute_sigma(m, t, q, r, max_new_tokens=int(args.max_new_tokens))[0]

    def _spec_fn(m: Any, t: Any, q: str, r: str, reference: str | None) -> float:
        return spectral.compute_sigma(m, t, q, r)[0]

    def _ult_fn(m: Any, t: Any, q: str, r: str, reference: str | None) -> float:
        return ultimate(m, t, q, r, reference=reference, max_new_tokens=int(args.max_new_tokens))[0]

    scorers: Dict[str, Callable[..., float]] = {
        "lsd": _lsd_fn,
        "hide": _hide_fn,
        "spectral": _spec_fn,
        "ultimate": _ult_fn,
    }

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

    all_out: Dict[str, Any] = {}

    try:

        def eval_rows(name: str, rows: List[Dict[str, Any]], label_fn: Callable[..., Tuple[int, str]]) -> Dict[str, float]:
            series: Dict[str, List[float]] = {k: [] for k in scorers}
            labels: List[int] = []
            for i, row in enumerate(rows):
                question = (row.get("question") or "").strip()
                if not question:
                    continue
                q_in = prism_wrap_question(question) if use_prism else question
                response = _greedy(model, tok, q_in, max_new_tokens=args.max_new_tokens, device=device)
                wrong, _tag = label_fn(row, response)
                labels.append(wrong)
                ref = (row.get("best_answer") or row.get("reference") or "").strip() or None
                scores = _score_row(scorers, model, tok, q_in, response, ref)
                for k, v in scores.items():
                    series[k].append(v)
                mark = "ok" if wrong == 0 else "x"
                print(
                    f"[{name} {i+1}/{len(rows)}] {mark} "
                    + " ".join(f"{k}={scores[k]:.3f}" for k in scorers)
                )
            y = np.array(labels, dtype=np.int64)
            return {k: _auroc(y, np.array(series[k], dtype=np.float64)) for k in scorers}

        # --- Holdout ---
        csv_path = repo / "benchmarks/sigma_gate_lsd/splits/holdout.csv"
        hold_rows: List[Dict[str, str]] = []
        with csv_path.open(newline="", encoding="utf-8") as fp:
            hold_rows = list(csv.DictReader(fp))
        if int(args.holdout_limit) > 0:
            hold_rows = hold_rows[: int(args.holdout_limit)]

        def _label_holdout(row: Dict[str, Any], response: str) -> Tuple[int, str]:
            ref = (row.get("best_answer") or "").strip()
            ok, _ = is_correct_semantic(response, ref, threshold=float(args.semantic_threshold))
            return (0 if ok else 1), "holdout"

        all_out["truthfulqa_holdout"] = eval_rows("holdout", hold_rows, _label_holdout)

        # --- TriviaQA ---
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

            all_out["triviaqa"] = eval_rows("triviaqa", trows, _label_trivia)
        except Exception as e:
            all_out["triviaqa"] = {"error": str(e)}

        # --- HaluEval generative ---
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

            all_out["halueval_generative"] = eval_rows("halueval", hrows, _label_halu)
        except Exception as e:
            all_out["halueval_generative"] = {"error": str(e)}

        training_cv = float("nan")
        try:
            training_cv = float(pickle.loads(probe.read_bytes())["manifest"].get("cv_auroc_mean", float("nan")))
        except Exception:
            pass

        summary = {
            "generator_model": args.model,
            "prism": use_prism,
            "spectral_laplacian_backend": str(args.spectral_backend),
            "training_cv_auroc_manifest": training_cv,
            "probe_path": str(probe),
            "aurocs_wrong_vs_score": all_out,
            "disclaimer": "HIDE official path follows github.com/C-anwoy/HIDE metric layout (vendored core in python/cos/hide_metric_upstream.py). AUROC is harness-only — calibrate before external claims.",
        }

        out_dir = repo / "benchmarks" / "sigma_gate_eval" / "results_ultimate"
        out_dir.mkdir(parents=True, exist_ok=True)
        (out_dir / "ultimate_summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")

        print(f"\n{'=' * 70}")
        print("  Ultimate eval (AUROC wrong vs score)")
        print(f"{'=' * 70}")
        for block_name, block in all_out.items():
            print(f"  {block_name}:")
            if isinstance(block, dict) and "error" in block and len(block) == 1:
                print(f"    error: {block['error']}")
                continue
            for k, v in block.items():
                print(f"    {k}: {v:.4f}" if isinstance(v, float) and not math.isnan(v) else f"    {k}: nan")
        print(f"{'=' * 70}")
    finally:
        lsd_gate.close()
        ultimate.close()


if __name__ == "__main__":
    main()
