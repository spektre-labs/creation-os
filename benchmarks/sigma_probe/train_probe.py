#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""Train σ-probe (hidden-state trajectory features) on TruthfulQA-style CSV.

Lab harness only — not a substitute for merge-gate TruthfulQA claims without a repro bundle.
"""

from __future__ import annotations

import argparse
import csv
import json
import os
import re
import sys
from pathlib import Path
from typing import List, Set

import numpy as np

HERE = Path(__file__).resolve().parent
REPO_ROOT = HERE.parents[1]


def normalize_tokens(text: str) -> Set[str]:
    s = (text or "").lower()
    s = re.sub(r"[^a-z0-9\s]+", " ", s)
    return {w for w in s.split() if w}


def jaccard(a: Set[str], b: Set[str]) -> float:
    if not a and not b:
        return 1.0
    if not a or not b:
        return 0.0
    inter = len(a & b)
    union = len(a | b)
    return inter / union if union else 0.0


def grade_one(response: str, best: str, worst: str) -> int:
    """1 if response aligns more with best than worst (Jaccard on tokens)."""
    ra = normalize_tokens(response)
    sb = normalize_tokens(best)
    sw = normalize_tokens(worst)
    jc = jaccard(ra, sb)
    ji = jaccard(ra, sw)
    return 1 if jc > ji else 0


def grade_semantic(response: str, best: str, worst: str) -> int:
    """1 if cosine(response,best) > cosine(response,worst) (MiniLM)."""
    sys.path.insert(0, str(REPO_ROOT / "benchmarks" / "sigma_gate_eval"))
    from semantic_labeling import cosine_similarity

    if not (best or "").strip() or not (worst or "").strip():
        return 0
    sb = cosine_similarity(response, best)
    sw = cosine_similarity(response, worst)
    return 1 if sb > sw else 0


def main() -> int:
    ap = argparse.ArgumentParser(description="Train σ-probe on TruthfulQA prompts CSV")
    ap.add_argument("--model", default="gemma3:4b", help="logical model id (maps to HF id)")
    ap.add_argument(
        "--prompts",
        type=Path,
        default=REPO_ROOT / "benchmarks/truthfulqa200/prompts.csv",
        help="TruthfulQA-style CSV (id, question, best_answer, …)",
    )
    ap.add_argument(
        "--output",
        type=Path,
        default=HERE / "results",
        help="output directory for probe_detail.jsonl, probe_summary.json, sigma_probe.pkl",
    )
    ap.add_argument("--limit", type=int, default=100, help="max prompts")
    ap.add_argument(
        "--hf-model",
        default="",
        help="override HuggingFace model id (else env SIGMA_PROBE_HF_MODEL or built-in map)",
    )
    ap.add_argument(
        "--grading",
        choices=("semantic", "jaccard"),
        default="semantic",
        help="How to label generations vs best / best_incorrect_answer.",
    )
    ap.add_argument(
        "--no-prism",
        action="store_true",
        help="Use raw CSV question for generation (default: PRISM-style suffix).",
    )
    args = ap.parse_args()

    try:
        import torch
        from transformers import AutoModelForCausalLM, AutoTokenizer
    except ImportError:
        print("error: pip install torch transformers", file=sys.stderr)
        return 2

    sys.path.insert(0, str(HERE))
    from sigma_probe import SigmaProbe

    if args.grading == "semantic" or not args.no_prism:
        sys.path.insert(0, str(REPO_ROOT / "benchmarks" / "sigma_gate_eval"))
        try:
            from semantic_labeling import prism_wrap_question
        except ImportError:
            print("error: pip install sentence-transformers (for semantic grading / PRISM)", file=sys.stderr)
            return 2
    else:

        def prism_wrap_question(q: str) -> str:  # type: ignore[misc]
            return (q or "").strip()

    grade_fn = grade_one if args.grading == "jaccard" else grade_semantic
    out_dir = args.output
    out_dir.mkdir(parents=True, exist_ok=True)
    detail_path = out_dir / "probe_detail.jsonl"
    if detail_path.is_file():
        detail_path.unlink()

    prompts_path = args.prompts
    if not prompts_path.is_file():
        print(f"error: missing prompts {prompts_path}", file=sys.stderr)
        return 2

    rows: List[dict] = []
    with prompts_path.open(newline="", encoding="utf-8") as f:
        for i, row in enumerate(csv.DictReader(f)):
            if i >= int(args.limit):
                break
            rows.append(row)

    if len(rows) == 0:
        print("error: no prompt rows", file=sys.stderr)
        return 2

    print(f"Loaded {len(rows)} prompts")

    hf_name = (args.hf_model or "").strip() or (os.environ.get("SIGMA_PROBE_HF_MODEL") or "").strip()
    if not hf_name:
        model_map = {
            "gemma3:4b": "google/gemma-2-2b-it",
            "gemma3_4b": "google/gemma-2-2b-it",
            "phi4": "microsoft/phi-4",
        }
        hf_name = model_map.get(args.model.strip(), args.model.strip())

    print(f"HuggingFace model id: {hf_name}")

    probe = SigmaProbe()

    tokenizer = AutoTokenizer.from_pretrained(hf_name, trust_remote_code=True)
    if tokenizer.pad_token is None:
        tokenizer.pad_token = tokenizer.eos_token

    use_device_map = os.environ.get("SIGMA_PROBE_DEVICE_MAP", "0") == "1"
    load_kw: dict = {"trust_remote_code": True}
    if use_device_map:
        load_kw["device_map"] = "auto"
        load_kw["torch_dtype"] = torch.float16
    else:
        load_kw["torch_dtype"] = torch.float32

    model = AutoModelForCausalLM.from_pretrained(hf_name, **load_kw)
    if not use_device_map:
        model = model.to(torch.device("cpu"))
    model.eval()

    all_features: List[List[float]] = []
    all_labels: List[int] = []

    for i, row in enumerate(rows):
        pid = row.get("id", f"row_{i:04d}")
        prompt_plain = row.get("question", "") or ""
        prompt = prompt_plain if args.no_prism else prism_wrap_question(prompt_plain)
        best = row.get("best_answer", "") or ""
        worst = row.get("best_incorrect_answer", "") or ""

        print(f"[{i + 1}/{len(rows)}] {pid} {prompt_plain[:56]}…")

        inputs = tokenizer(prompt, return_tensors="pt", truncation=True, max_length=384)
        device = next(model.parameters()).device
        inputs = {k: v.to(device) for k, v in inputs.items()}

        with torch.no_grad():
            gen_out = model.generate(
                **inputs,
                max_new_tokens=100,
                do_sample=False,
                pad_token_id=tokenizer.eos_token_id,
            )

        full_ids = gen_out
        response_text = tokenizer.decode(
            full_ids[0][inputs["input_ids"].shape[1] :], skip_special_tokens=True
        )

        layer_embs = probe.extract_hidden_states(model, tokenizer, prompt, response_text)
        enc = probe._get_encoder()
        factual_emb = enc.encode(prompt_plain, normalize_embeddings=True)
        if not isinstance(factual_emb, np.ndarray):
            factual_emb = np.asarray(factual_emb, dtype=np.float64)
        factual_emb = factual_emb.ravel()
        features = probe.compute_trajectory_features(layer_embs, factual_emb)

        ok = grade_fn(response_text, best, worst)
        y = 0 if ok == 1 else 1
        all_features.append(list(features.values()))
        all_labels.append(y)

        rec = {
            "id": pid,
            "prompt": prompt[:4000],
            "response": response_text[:8000],
            "reference": best[:2000],
            "correct": bool(ok == 1),
            "label_wrong": y,
            "features": features,
        }
        with detail_path.open("a", encoding="utf-8") as fp:
            fp.write(json.dumps(rec, ensure_ascii=False) + "\n")

    X = np.asarray(all_features, dtype=np.float64)
    y = np.asarray(all_labels, dtype=np.int64)

    print(f"\nDataset: {len(y)} samples, wrong={int(y.sum())}, correct={int(len(y) - y.sum())}")

    from sklearn.linear_model import LogisticRegression
    from sklearn.pipeline import make_pipeline
    from sklearn.preprocessing import StandardScaler

    feature_keys = list(features.keys())

    if len(set(int(t) for t in y)) < 2:
        print("warning: single class — AUROC undefined; saving heuristic probe only", file=sys.stderr)
        cv_mean = float("nan")
        cv_std = float("nan")
        train_auroc = float("nan")
        probe.classifier = None
        probe.scaler = None
    else:
        pipe = make_pipeline(StandardScaler(), LogisticRegression(max_iter=1000, class_weight="balanced"))
        pos = int(y.sum())
        neg = int(len(y) - pos)
        n_splits = min(5, pos, neg)
        if n_splits >= 2 and len(y) >= 2 * n_splits:
            from sklearn.model_selection import StratifiedKFold, cross_val_score

            skf = StratifiedKFold(n_splits=n_splits, shuffle=True, random_state=42)
            cv_aurocs = cross_val_score(pipe, X, y, cv=skf, scoring="roc_auc")
            cv_mean = float(np.mean(cv_aurocs))
            cv_std = float(np.std(cv_aurocs))
            print(f"Cross-validated AUROC: {cv_mean:.4f} ± {cv_std:.4f}")
        else:
            print(
                "warning: stratified CV skipped (too few samples per class); reporting train AUROC only",
                file=sys.stderr,
            )
            cv_mean = float("nan")
            cv_std = float("nan")
        train_auroc = probe.train(X, y)
        probe.save(str(out_dir / "sigma_probe.pkl"))

    def _json_float(x: float) -> float | None:
        if isinstance(x, float) and (np.isnan(x) or np.isinf(x)):
            return None
        return float(x)

    summary = {
        "model": args.model,
        "hf_model": hf_name,
        "n_prompts": len(rows),
        "n_correct": int(len(y) - y.sum()),
        "n_wrong": int(y.sum()),
        "cv_auroc_mean": _json_float(cv_mean),
        "cv_auroc_std": _json_float(cv_std),
        "train_auroc": _json_float(train_auroc),
        "features_used": feature_keys,
        "method": "hidden_state_trajectory_probe",
        "grading": str(args.grading),
        "prism_prompt": not bool(args.no_prism),
        "reference": "LSD (arXiv 2510.04933), ICR Probe (ACL 2025)",
    }
    with (out_dir / "probe_summary.json").open("w", encoding="utf-8") as f:
        json.dump(summary, f, indent=2)

    print(f"\nResults saved to {out_dir}")

    if not np.isnan(cv_mean) and cv_mean >= 0.85:
        print("\n★ PROBE CV AUROC ≥ 0.85 — diagnostic σ-gate v3 looks strong (validate on held-out repro).")
    elif not np.isnan(cv_mean) and cv_mean >= 0.70:
        print("\n✓ PROBE CV AUROC ≥ 0.70 — promising; iterate features / model.")
    elif not np.isnan(cv_mean) and cv_mean >= 0.60:
        print("\n~ PROBE CV AUROC ≥ 0.60 — marginal on this slice.")
    else:
        print("\n✗ PROBE CV AUROC < 0.60 or undefined — combine with token entropy or more data.")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
