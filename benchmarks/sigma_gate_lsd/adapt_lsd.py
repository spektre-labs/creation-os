#!/usr/bin/env python3
"""
Adapt Sirraya LSD to Creation OS TruthfulQA CSV: contrastive projection training,
trajectory features, 5-fold CV AUROC on a logistic head, then save sigma_gate_lsd.pkl.

TruthfulQA CSV may expose multiple incorrect strings via best_incorrect_answer,
incorrect_answers (semicolon-separated), or incorrect_answer. If there are not
enough hallucination pairs to reach --min-pairs / 2, optional GPT-2 sampling
fills the gap (same HF model as contrastive training).
"""
from __future__ import annotations

import argparse
import csv
import json
import os
import pickle
import random
import sys
from pathlib import Path
from typing import Any, Dict, List, Set, Tuple

import numpy as np


Pair = Tuple[str, str, str]  # (question, answer, "factual"|"hallucination")


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def _norm(s: str) -> str:
    return " ".join(s.lower().split())


def _incorrect_parts(row: Dict[str, str]) -> List[str]:
    parts: List[str] = []
    for key in ("incorrect_answers", "incorrect_answer", "best_incorrect_answer"):
        raw = row.get(key) or ""
        if not str(raw).strip():
            continue
        for piece in str(raw).split(";"):
            t = piece.strip()
            if t:
                parts.append(t)
    out: List[str] = []
    seen: Set[str] = set()
    for p in parts:
        k = _norm(p)
        if k not in seen:
            seen.add(k)
            out.append(p)
    return out


def _load_rows(path: Path, limit: int | None) -> List[Dict[str, str]]:
    rows: List[Dict[str, str]] = []
    with path.open(newline="", encoding="utf-8") as fp:
        for row in csv.DictReader(fp):
            rows.append({k: (v or "") if v is not None else "" for k, v in row.items()})
    if limit is not None:
        rows = rows[: int(limit)]
    return rows


def _rows_to_pair_lists(rows: List[Dict[str, str]]) -> Tuple[List[Pair], List[Pair]]:
    factual: List[Pair] = []
    hallu: List[Pair] = []
    for row in rows:
        q = (row.get("question") or "").strip()
        best = (row.get("best_answer") or row.get("answer") or "").strip()
        if q and best:
            factual.append((q, best, "factual"))
        for wrong in _incorrect_parts(row):
            if q and wrong:
                hallu.append((q, wrong, "hallucination"))
    return factual, hallu


def _generate_hallucinations(
    rows: List[Dict[str, str]],
    existing: List[Pair],
    model_name: str,
    need: int,
    temperature: float,
    max_new_tokens: int,
    max_rounds: int,
    rng: random.Random,
    device: str,
) -> List[Pair]:
    if need <= 0:
        return []

    try:
        import torch
        from transformers import AutoModelForCausalLM, AutoTokenizer
    except ImportError as e:
        print(f"Cannot import torch/transformers for augmentation: {e}", file=sys.stderr)
        return []

    tok = AutoTokenizer.from_pretrained(model_name)
    if tok.pad_token is None:
        tok.pad_token = tok.eos_token
    model = AutoModelForCausalLM.from_pretrained(model_name).to(device).eval()

    seen: Set[Tuple[str, str]] = set()
    for p in existing:
        if p[2] == "hallucination":
            seen.add((_norm(p[0]), _norm(p[1])))

    new_pairs: List[Pair] = []
    qs_best: List[Tuple[str, str]] = []
    for row in rows:
        q = (row.get("question") or "").strip()
        best = (row.get("best_answer") or row.get("answer") or "").strip()
        if q and best:
            qs_best.append((q, best))

    if not qs_best:
        return []

    prompt_tmpl = (
        "Question: {q}\n"
        "Write one plausible but incorrect or misleading answer in a single sentence.\n"
        "Answer:"
    )

    rounds = 0
    while len(new_pairs) < need and rounds < max_rounds:
        rounds += 1
        rng.shuffle(qs_best)
        for q, best in qs_best:
            if len(new_pairs) >= need:
                break
            prompt = prompt_tmpl.format(q=q)
            inputs = tok(prompt, return_tensors="pt").to(device)
            with torch.inference_mode():
                out = model.generate(
                    **inputs,
                    max_new_tokens=max_new_tokens,
                    do_sample=True,
                    temperature=temperature,
                    top_p=0.92,
                    pad_token_id=tok.pad_token_id,
                )
            gen = tok.decode(out[0][inputs["input_ids"].shape[1] :], skip_special_tokens=True).strip()
            gen = gen.split("\n")[0].strip()
            if len(gen) < 12:
                continue
            nb, ng = _norm(best), _norm(gen)
            if ng == nb or ng in nb or nb in ng:
                continue
            key = (_norm(q), ng)
            if key in seen:
                continue
            seen.add(key)
            new_pairs.append((q, gen, "hallucination"))

    return new_pairs


def _upsample(pairs: List[Pair], target: int, rng: random.Random) -> List[Pair]:
    if len(pairs) >= target:
        rng.shuffle(pairs)
        return pairs[:target]
    out = pairs.copy()
    base = pairs.copy()
    while len(out) < target:
        rng.shuffle(base)
        out.extend(base)
    rng.shuffle(out)
    return out[:target]


def build_balanced_pairs(
    rows: List[Dict[str, str]],
    min_pairs: int,
    hf_model: str,
    gen_temperature: float,
    augment_generate: bool,
    rng: random.Random,
) -> Tuple[List[Pair], Dict[str, Any]]:
    factual, hallu = _rows_to_pair_lists(rows)
    meta: Dict[str, Any] = {
        "n_factual_raw": len(factual),
        "n_hallucination_raw": len(hallu),
        "min_pairs_target": min_pairs,
    }

    half = max(min_pairs // 2, 1)
    if len(hallu) < half and augment_generate:
        need = half - len(hallu)
        try:
            import torch

            dev = "cuda" if torch.cuda.is_available() else "cpu"
        except ImportError:
            dev = "cpu"
        extra = _generate_hallucinations(
            rows,
            hallu,
            hf_model,
            need=need,
            temperature=gen_temperature,
            max_new_tokens=72,
            max_rounds=max(200, need * 4),
            rng=rng,
            device=dev,
        )
        hallu.extend(extra)
        meta["n_hallucination_generated"] = len(extra)
    else:
        meta["n_hallucination_generated"] = 0

    factual_u = _upsample(factual, half, rng)
    rng.shuffle(hallu)
    hallu_u = hallu[:half] if len(hallu) >= half else _upsample(hallu, half, rng)

    balanced = factual_u + hallu_u
    rng.shuffle(balanced)
    meta["n_factual_balanced"] = len(factual_u)
    meta["n_hallucination_balanced"] = len(hallu_u)
    meta["n_pairs_total"] = len(balanced)
    return balanced, meta


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--prompts",
        type=Path,
        default=_repo_root() / "benchmarks/truthfulqa200/prompts.csv",
    )
    ap.add_argument(
        "--output",
        type=Path,
        default=_repo_root() / "benchmarks/sigma_gate_lsd/results/latest",
    )
    ap.add_argument("--hf-model", default=os.environ.get("CREATION_LSD_HF_MODEL", "gpt2"))
    ap.add_argument(
        "--epochs",
        type=int,
        default=int(os.environ.get("CREATION_LSD_EPOCHS", "15")),
    )
    ap.add_argument("--train-frac", type=float, default=0.8)
    ap.add_argument("--limit", type=int, default=None)
    ap.add_argument(
        "--min-pairs",
        type=int,
        default=int(os.environ.get("CREATION_LSD_MIN_PAIRS", "800")),
        help="Minimum total pairs after balancing (~half factual, half hallucination).",
    )
    ap.add_argument("--batch-size", type=int, default=int(os.environ.get("CREATION_LSD_BATCH_SIZE", "32")))
    ap.add_argument(
        "--learning-rate",
        type=float,
        default=float(os.environ.get("CREATION_LSD_LEARNING_RATE", "1e-3")),
    )
    ap.add_argument("--margin", type=float, default=float(os.environ.get("CREATION_LSD_MARGIN", "0.3")))
    ap.add_argument(
        "--gen-temperature",
        type=float,
        default=float(os.environ.get("CREATION_LSD_GEN_TEMPERATURE", "1.2")),
    )
    ap.add_argument(
        "--no-augment-generate",
        action="store_true",
        help="Do not sample extra wrong answers with GPT-2 when CSV negatives are short.",
    )
    args = ap.parse_args()

    rng = random.Random(42)
    repo = _repo_root()
    lsd_root = repo / "external" / "lsd"
    if not lsd_root.is_dir():
        print("Missing external/lsd — clone Sirraya_LSD_Code.git into external/lsd", file=sys.stderr)
        sys.exit(2)

    # Resolve before chdir(external/lsd) so relative paths stay under the repo root.
    p_q = Path(args.prompts).expanduser()
    args.prompts = p_q.resolve() if p_q.is_absolute() else (repo / p_q).resolve()
    p_o = Path(args.output).expanduser()
    args.output = p_o.resolve() if p_o.is_absolute() else (repo / p_o).resolve()

    rows = _load_rows(args.prompts, args.limit)
    pairs, aug_meta = build_balanced_pairs(
        rows,
        min_pairs=args.min_pairs,
        hf_model=args.hf_model,
        gen_temperature=args.gen_temperature,
        augment_generate=not args.no_augment_generate,
        rng=rng,
    )

    if len(pairs) < 16:
        print("Not enough balanced pairs.", file=sys.stderr)
        sys.exit(3)

    n_train = int(len(pairs) * args.train_frac)
    train_pairs = pairs[:n_train]
    all_pairs = pairs

    prev = os.getcwd()
    os.chdir(lsd_root)
    sys.path.insert(0, str(lsd_root))

    from lsd.core.config import LayerwiseSemanticDynamicsConfig, OperationMode  # noqa: PLC0415
    from lsd.models.feature_extractor import analyze_layerwise_dynamics  # noqa: PLC0415
    from lsd.models.manager import train_projection_heads  # noqa: PLC0415
    from sklearn.linear_model import LogisticRegression  # noqa: PLC0415
    from sklearn.model_selection import StratifiedKFold, cross_val_score  # noqa: PLC0415
    from sklearn.preprocessing import StandardScaler  # noqa: PLC0415

    cfg = LayerwiseSemanticDynamicsConfig(
        model_name=args.hf_model,
        truth_encoder_name="sentence-transformers/all-MiniLM-L6-v2",
        datasets=[],
        num_pairs=len(train_pairs),
        epochs=args.epochs,
        batch_size=args.batch_size,
        learning_rate=args.learning_rate,
        margin=args.margin,
        mode=OperationMode.HYBRID,
        use_pretrained=False,
        train_test_split=0.85,
    )
    cfg.datasets = []
    cfg.num_pairs = len(train_pairs)

    print(
        f"LSD contrastive train: model={args.hf_model} train_pairs={len(train_pairs)} "
        f"epochs={args.epochs} batch={args.batch_size} lr={args.learning_rate} margin={args.margin}"
    )
    print(f"Augmentation meta: {json.dumps(aug_meta)}")
    mm = train_projection_heads(cfg, train_pairs)

    print(f"Trajectory features on {len(all_pairs)} pairs (for 5-fold CV + final head)")
    df, _, _ = analyze_layerwise_dynamics(all_pairs, mm)

    feature_columns = [
        "final_alignment",
        "mean_alignment",
        "max_alignment",
        "convergence_layer",
        "stability",
        "alignment_gain",
        "mean_velocity",
        "mean_acceleration",
        "oscillation_count",
    ]
    cols = [c for c in feature_columns if c in df.columns]
    X = np.nan_to_num(df[cols].values.astype(np.float64))
    y = np.array([1 if lab == "factual" else 0 for lab in df["label"]])
    if len(np.unique(y)) < 2:
        print("Single class in feature matrix.", file=sys.stderr)
        os.chdir(prev)
        sys.exit(4)

    scaler = StandardScaler()
    Xs = scaler.fit_transform(X)
    clf_template = LogisticRegression(random_state=42, max_iter=2000, class_weight="balanced")
    pos, neg = int(y.sum()), int(len(y) - y.sum())
    n_splits = min(5, pos, neg)
    if n_splits < 2:
        print("Not enough per-class samples for stratified 5-fold; skipping CV.", file=sys.stderr)
        cv_mean, cv_std = float("nan"), float("nan")
    else:
        skf = StratifiedKFold(n_splits=n_splits, shuffle=True, random_state=42)
        try:
            cv_scores = cross_val_score(clf_template, Xs, y, cv=skf, scoring="roc_auc")
            cv_mean = float(np.mean(cv_scores))
            cv_std = float(np.std(cv_scores))
        except Exception as ex:
            print(f"CV failed: {ex}", file=sys.stderr)
            cv_mean = float("nan")
            cv_std = float("nan")

    clf = LogisticRegression(random_state=42, max_iter=2000, class_weight="balanced")
    clf.fit(Xs, y)

    args.output.mkdir(parents=True, exist_ok=True)
    models_dir = args.output / "models"
    models_dir.mkdir(parents=True, exist_ok=True)
    import shutil  # noqa: PLC0415

    vendor_models = lsd_root / "layerwise_semantic_dynamics_system" / "models"
    if vendor_models.is_dir():
        for p in vendor_models.glob("*.pt"):
            shutil.copy2(p, models_dir / p.name)

    manifest: dict[str, Any] = {
        "hf_model": args.hf_model,
        "truth_encoder_name": cfg.truth_encoder_name,
        "lsd_vendor_root": str(lsd_root.resolve()),
        "models_dir": str(models_dir.resolve()),
        "n_train_pairs_contrastive": len(train_pairs),
        "n_pairs_features": len(all_pairs),
        "cv_auroc_mean": cv_mean,
        "cv_auroc_std": cv_std,
        "feature_columns": cols,
        "text_side": "question",
        "truth_side": "reference_answer_string",
        "augmentation": aug_meta,
        "epochs": args.epochs,
        "batch_size": args.batch_size,
        "learning_rate": args.learning_rate,
        "margin": args.margin,
        "min_pairs_requested": args.min_pairs,
    }

    gate = {"classifier": clf, "scaler": scaler, "feature_columns": cols}
    bundle = {"manifest": manifest, "gate": gate}
    (args.output / "sigma_gate_lsd.pkl").write_bytes(pickle.dumps(bundle))
    (args.output / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    summary = {
        "cv_auroc_mean": cv_mean,
        "cv_auroc_std": cv_std,
        "hf_model": args.hf_model,
        "n_pairs": len(all_pairs),
        "augmentation": aug_meta,
    }
    (args.output / "lsd_summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")

    print(json.dumps(summary, indent=2))
    os.chdir(prev)


if __name__ == "__main__":
    main()
