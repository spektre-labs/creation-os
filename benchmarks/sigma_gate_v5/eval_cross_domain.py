#!/usr/bin/env python3
"""
Leave-one-source-out: train LSD on CSV rows excluding held-out ``source``,
then score held-out labeled rows with the new probe (AUROC wrong vs σ).

Heavy: one ``adapt_lsd.py`` run per distinct source with enough train rows.
"""
from __future__ import annotations

import argparse
import csv
import json
import math
import os
import subprocess
import sys
from collections import defaultdict
from pathlib import Path
from typing import Any, Dict, List, Tuple

import numpy as np

try:
    from sklearn.metrics import roc_auc_score
except ImportError as e:
    print("sklearn required:", e, file=sys.stderr)
    sys.exit(2)


def _repo() -> Path:
    return Path(__file__).resolve().parents[2]


def _load_csv_rows(path: Path) -> List[Dict[str, str]]:
    with path.open(encoding="utf-8", newline="") as fp:
        return list(csv.DictReader(fp))


def _write_subset_csv(rows: List[Dict[str, str]], path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if not rows:
        path.write_text("", encoding="utf-8")
        return
    fields = list(rows[0].keys())
    with path.open("w", encoding="utf-8", newline="") as fp:
        w = csv.DictWriter(fp, fieldnames=fields)
        w.writeheader()
        for r in rows:
            w.writerow(r)


def eval_one_probe(
    probe: Path,
    held_rows: List[Dict[str, Any]],
) -> Tuple[float, int]:
    repo = _repo()
    sys.path.insert(0, str(repo / "python"))

    from cos.sigma_gate import SigmaGate

    if not probe.is_file() or not held_rows:
        return float("nan"), 0

    gate = SigmaGate(str(probe))
    try:
        sigmas: List[float] = []
        wrongs: List[int] = []
        for r in held_rows:
            qm = (r.get("question_for_model") or "").strip()
            resp = (r.get("response") or "").strip()
            ok = bool(r.get("semantic_correct"))
            w = 0 if ok else 1
            # Probe scores (prompt, response); outer HF model is not used by LSD pickle path.
            sigma, _ = gate(None, None, qm, resp)
            sigmas.append(float(sigma))
            wrongs.append(w)
        y = np.array(wrongs, dtype=np.int64)
        s = np.array(sigmas, dtype=np.float64)
        if len(np.unique(y)) < 2:
            return float("nan"), len(held_rows)
        return float(roc_auc_score(y, s)), len(held_rows)
    finally:
        gate.close()


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--epochs", type=int, default=10)
    ap.add_argument("--min-pairs", type=int, default=240)
    ap.add_argument("--min-train-rows", type=int, default=24)
    args = ap.parse_args()
    repo = _repo()

    csv_path = repo / "benchmarks/sigma_gate_v5/intermediate/multi_contrastive.csv"
    lab_path = repo / "benchmarks/sigma_gate_v5/intermediate/labeled_rows.jsonl"
    if not csv_path.is_file() or not lab_path.is_file():
        print("Run generate + create_contrastive_pairs first.", file=sys.stderr)
        sys.exit(2)

    csv_rows = _load_csv_rows(csv_path)
    by_source: Dict[str, List[Dict[str, str]]] = defaultdict(list)
    for r in csv_rows:
        by_source[(r.get("source") or "unknown").strip()].append(r)

    held: Dict[str, List[Dict[str, Any]]] = defaultdict(list)
    with lab_path.open(encoding="utf-8") as fp:
        for line in fp:
            line = line.strip()
            if not line:
                continue
            o = json.loads(line)
            held[(o.get("source") or "unknown").strip()].append(o)

    sources = sorted(set(by_source.keys()) & set(held.keys()))
    per_auroc: Dict[str, float] = {}
    env = os.environ.copy()
    env["PYTHONPATH"] = str(repo / "python")

    for src in sources:
        train_rows = [r for r in csv_rows if (r.get("source") or "").strip() != src]
        if len(train_rows) < int(args.min_train_rows):
            print(f"Skip LOO {src}: only {len(train_rows)} train rows", file=sys.stderr)
            continue
        tmp = repo / f"benchmarks/sigma_gate_v5/intermediate/loo_train_minus_{src}.csv"
        _write_subset_csv(train_rows, tmp)
        ntrain = len(train_rows)
        out_dir = repo / f"benchmarks/sigma_gate_v5/results/loo_minus_{src}"
        cmd = [
            sys.executable,
            str(repo / "benchmarks/sigma_gate_lsd/adapt_lsd.py"),
            "--prompts",
            str(tmp),
            "--output",
            str(out_dir),
            "--limit",
            str(ntrain),
            "--epochs",
            str(int(args.epochs)),
            "--min-pairs",
            str(int(args.min_pairs)),
        ]
        print("=== LOO train excluding", src, "===")
        subprocess.run(cmd, check=True, cwd=str(repo), env=env)
        probe = out_dir / "sigma_gate_lsd.pkl"
        au, n = eval_one_probe(probe, held[src])
        per_auroc[src] = au
        print(f"  held-out {src}: AUROC={au:.4f} n_scored={n}")

    vals = [v for v in per_auroc.values() if not math.isnan(v)]
    mean_auroc = float(sum(vals) / len(vals)) if vals else float("nan")
    summary = {
        "per_dataset_auroc": per_auroc,
        "mean_auroc": mean_auroc,
        "sources": sources,
        "labeling": "semantic_on_labeled_jsonl_wrong_vs_regenerated_sigma",
    }
    out_json = repo / "benchmarks/sigma_gate_v5/results/v5_summary.json"
    out_json.parent.mkdir(parents=True, exist_ok=True)
    out_json.write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print(f"Wrote {out_json}")


if __name__ == "__main__":
    main()
