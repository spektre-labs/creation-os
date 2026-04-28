#!/usr/bin/env python3
"""
Train Sirraya LSD contrastive projection heads on Creation OS TruthfulQA CSV rows,
then evaluate AUROC (hybrid / supervised branch) on a held-out slice.

Mirrors vendor TruthfulQA pairing: (question, reference_answer, factual|hallucination).
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
from typing import List, Tuple

import numpy as np


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def _load_pairs_csv(path: Path, limit: int | None) -> List[Tuple[str, str, str]]:
    rows: List[dict] = []
    with path.open(newline="", encoding="utf-8") as fp:
        r = csv.DictReader(fp)
        for row in r:
            rows.append(row)
    if limit is not None:
        rows = rows[: int(limit)]
    pairs: List[Tuple[str, str, str]] = []
    for row in rows:
        q = (row.get("question") or "").strip()
        best = (row.get("best_answer") or "").strip()
        worst = (row.get("best_incorrect_answer") or "").strip()
        if q and best:
            pairs.append((q, best, "factual"))
        if q and worst:
            pairs.append((q, worst, "hallucination"))
    factual = [p for p in pairs if p[2] == "factual"]
    hallu = [p for p in pairs if p[2] == "hallucination"]
    n = min(len(factual), len(hallu))
    random.seed(42)
    random.shuffle(factual)
    random.shuffle(hallu)
    balanced = factual[:n] + hallu[:n]
    random.shuffle(balanced)
    return balanced


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
        default=_repo_root() / "benchmarks/sigma_probe_lsd/results/creation_lsd_run",
    )
    ap.add_argument("--hf-model", default=os.environ.get("CREATION_LSD_HF_MODEL", "gpt2"))
    ap.add_argument("--epochs", type=int, default=int(os.environ.get("CREATION_LSD_EPOCHS", "8")))
    ap.add_argument("--train-frac", type=float, default=0.8)
    ap.add_argument("--limit", type=int, default=None, help="Max CSV rows (before pair expansion)")
    args = ap.parse_args()

    repo = _repo_root()
    lsd_root = repo / "external" / "lsd"
    if not lsd_root.is_dir():
        print("Missing external/lsd", file=sys.stderr)
        sys.exit(2)

    pairs = _load_pairs_csv(args.prompts.resolve(), args.limit)
    if len(pairs) < 8:
        print("Not enough balanced pairs after CSV load.", file=sys.stderr)
        sys.exit(3)

    n_train = int(len(pairs) * args.train_frac)
    train_pairs = pairs[:n_train]
    val_pairs = pairs[n_train:]

    prev = os.getcwd()
    os.chdir(lsd_root)
    sys.path.insert(0, str(lsd_root))

    from lsd.core.config import LayerwiseSemanticDynamicsConfig, OperationMode  # noqa: PLC0415
    from lsd.evaluation.evaluator import ComprehensiveEvaluator  # noqa: PLC0415
    from lsd.models.feature_extractor import analyze_layerwise_dynamics  # noqa: PLC0415
    from lsd.models.manager import train_projection_heads  # noqa: PLC0415

    cfg = LayerwiseSemanticDynamicsConfig(
        model_name=args.hf_model,
        truth_encoder_name="sentence-transformers/all-MiniLM-L6-v2",
        datasets=[],  # noqa: RUF012 — vendor __post_init__ replaces None only
        num_pairs=len(train_pairs),
        epochs=args.epochs,
        batch_size=4,
        mode=OperationMode.HYBRID,
        use_pretrained=False,
        train_test_split=0.85,
    )
    cfg.datasets = []
    cfg.num_pairs = len(train_pairs)

    print(f"Training LSD heads: model={args.hf_model} train_pairs={len(train_pairs)} epochs={args.epochs}")
    mm = train_projection_heads(cfg, train_pairs)

    print(f"Analyzing validation pairs: {len(val_pairs)}")
    df, _, _ = analyze_layerwise_dynamics(val_pairs, mm)
    ev = ComprehensiveEvaluator(cfg)
    try:
        hybrid = ev.evaluate_hybrid(df)
    except Exception as ex:
        print(f"evaluate_hybrid failed: {ex}", file=sys.stderr)
        hybrid = {"supervised": {}, "unsupervised": {}, "hybrid_metrics": {}}
    sup = hybrid.get("supervised") or {}
    best_auroc = 0.0
    best_name = ""
    for name, m in sup.items():
        a = float(m.get("auroc", 0.0))
        if a > best_auroc:
            best_auroc = a
            best_name = name

    from sklearn.linear_model import LogisticRegression  # noqa: PLC0415
    from sklearn.model_selection import cross_val_score, train_test_split  # noqa: PLC0415
    from sklearn.preprocessing import StandardScaler  # noqa: PLC0415

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
    if len(set(y.tolist())) < 2:
        print("Validation has a single class; AUROC undefined.", file=sys.stderr)
        os.chdir(prev)
        sys.exit(4)

    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42, stratify=y)
    scaler = StandardScaler()
    X_train_s = scaler.fit_transform(X_train)
    X_test_s = scaler.transform(X_test)
    clf = LogisticRegression(random_state=42, max_iter=2000, class_weight="balanced")
    clf.fit(X_train_s, y_train)
    try:
        cv_scores = cross_val_score(clf, X_train_s, y_train, cv=min(5, len(y_train) // 2), scoring="roc_auc")
        cv_mean = float(np.mean(cv_scores))
        cv_std = float(np.std(cv_scores))
    except Exception:
        cv_mean = float("nan")
        cv_std = float("nan")

    args.output.mkdir(parents=True, exist_ok=True)
    models_dir = args.output / "models"
    models_dir.mkdir(parents=True, exist_ok=True)
    import shutil  # noqa: PLC0415

    vendor_models = lsd_root / "layerwise_semantic_dynamics_system" / "models"
    if vendor_models.is_dir():
        for p in vendor_models.glob("*.pt"):
            shutil.copy2(p, models_dir / p.name)

    gate = {"classifier": clf, "scaler": scaler, "feature_columns": cols}
    (args.output / "gate_head.pkl").write_bytes(pickle.dumps(gate))

    manifest = {
        "hf_model": args.hf_model,
        "truth_encoder_name": cfg.truth_encoder_name,
        "lsd_vendor_root": str(lsd_root.resolve()),
        "models_dir": str(models_dir.resolve()),
        "n_train_pairs": len(train_pairs),
        "n_val_pairs": len(val_pairs),
        "best_supervised_auroc_val_eval": best_auroc,
        "best_supervised_method": best_name,
        "cv_auroc_mean": cv_mean,
        "cv_auroc_std": cv_std,
        "text_side": "question",
        "truth_side": "reference_answer_string",
    }
    (args.output / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    summary = {**manifest, "hybrid_keys": list(hybrid.keys())}
    (args.output / "train_summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")

    print(json.dumps({"cv_auroc_mean": cv_mean, "cv_auroc_std": cv_std, "val_best_auroc": best_auroc}, indent=2))
    os.chdir(prev)


if __name__ == "__main__":
    main()
