#!/usr/bin/env python3
"""
Create train / holdout CSV splits for sigma-gate LSD validation.

Default: stratify by the ``category`` column (TruthfulQA200 schema) so each
category contributes ~70% to train and ~30% to holdout. The holdout rows are
not used by ``adapt_lsd.py`` when training from ``train.csv``.

Reproducible: ``random.seed(42)`` for shuffles within strata.
"""
from __future__ import annotations

import argparse
import csv
import json
import random
import sys
from collections import defaultdict
from pathlib import Path
from typing import Any, Dict, List, Tuple


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def _load_rows(path: Path) -> List[Dict[str, str]]:
    rows: List[Dict[str, str]] = []
    with path.open(newline="", encoding="utf-8") as fp:
        for row in csv.DictReader(fp):
            rows.append({k: (v or "") if v is not None else "" for k, v in row.items()})
    return rows


def stratified_split(
    rows: List[Dict[str, str]],
    train_frac: float,
    rng: random.Random,
) -> Tuple[List[Dict[str, str]], List[Dict[str, str]]]:
    by_cat: Dict[str, List[Dict[str, str]]] = defaultdict(list)
    for row in rows:
        by_cat[row.get("category") or "_"].append(row)

    train: List[Dict[str, str]] = []
    holdout: List[Dict[str, str]] = []
    for _cat, items in sorted(by_cat.items(), key=lambda x: x[0]):
        xs = items.copy()
        rng.shuffle(xs)
        n = len(xs)
        if n == 1:
            train.append(xs[0])
            continue
        k = int(round(n * train_frac))
        if k == 0:
            k = 1
        if k >= n:
            k = n - 1
        train.extend(xs[:k])
        holdout.extend(xs[k:])

    rng.shuffle(train)
    rng.shuffle(holdout)
    return train, holdout


def random_split(
    rows: List[Dict[str, str]],
    train_frac: float,
    rng: random.Random,
) -> Tuple[List[Dict[str, str]], List[Dict[str, str]]]:
    xs = rows.copy()
    rng.shuffle(xs)
    k = int(round(len(xs) * train_frac))
    k = max(1, min(k, len(xs) - 1)) if len(xs) >= 2 else max(1, k)
    return xs[:k], xs[k:]


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--input",
        type=Path,
        default=None,
        help="Source CSV (default: benchmarks/truthfulqa200/prompts.csv).",
    )
    ap.add_argument(
        "--out-dir",
        type=Path,
        default=None,
        help="Output directory (default: benchmarks/sigma_gate_lsd/splits).",
    )
    ap.add_argument("--train-frac", type=float, default=0.7)
    ap.add_argument("--seed", type=int, default=42)
    ap.add_argument(
        "--mode",
        choices=("stratify_category", "random"),
        default="stratify_category",
        help="stratify_category: ~70/30 per category. random: single global shuffle.",
    )
    args = ap.parse_args()

    repo = _repo_root()
    src = args.input or (repo / "benchmarks/truthfulqa200/prompts.csv")
    src = src if src.is_absolute() else (repo / src).resolve()
    out_dir = args.out_dir or (repo / "benchmarks/sigma_gate_lsd/splits")
    out_dir = out_dir if out_dir.is_absolute() else (repo / out_dir).resolve()

    if not src.is_file():
        print(f"Missing input CSV: {src}", file=sys.stderr)
        sys.exit(2)

    rows = _load_rows(src)
    if len(rows) < 4:
        print("Need at least 4 rows to split.", file=sys.stderr)
        sys.exit(3)

    rng = random.Random(int(args.seed))
    if args.mode == "stratify_category":
        train, holdout = stratified_split(rows, float(args.train_frac), rng)
    else:
        train, holdout = random_split(rows, float(args.train_frac), rng)

    out_dir.mkdir(parents=True, exist_ok=True)
    fieldnames = list(rows[0].keys())

    for name, data in (("train", train), ("holdout", holdout)):
        path = out_dir / f"{name}.csv"
        with path.open("w", newline="", encoding="utf-8") as fp:
            w = csv.DictWriter(fp, fieldnames=fieldnames)
            w.writeheader()
            w.writerows(data)

    manifest: Dict[str, Any] = {
        "seed": int(args.seed),
        "mode": args.mode,
        "train_frac": float(args.train_frac),
        "source_csv": str(src),
        "total": len(rows),
        "train": len(train),
        "holdout": len(holdout),
        "holdout_ids_sample": [r.get("id", "") for r in holdout[:8]],
    }
    (out_dir / "split_manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")

    print(f"Train: {len(train)}, Holdout: {len(holdout)}")
    print(f"Wrote {out_dir / 'train.csv'} and {out_dir / 'holdout.csv'}")


if __name__ == "__main__":
    main()
