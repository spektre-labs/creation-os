#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""Print a publication-style metrics box from benchmarks/graded/graded_results.csv.

Expects columns including sigma, is_correct, action (ABSTAIN vs non-abstain).
Does not invent model or hardware strings: override with env or --model/--hardware.
"""
from __future__ import annotations

import argparse
import csv
import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_CSV = ROOT / "benchmarks" / "graded" / "graded_results.csv"


def load_rows(path: Path) -> list[dict]:
    rows: list[dict] = []
    with path.open(newline="", encoding="utf-8") as f:
        for r in csv.DictReader(f):
            try:
                rows.append(
                    {
                        "sigma": float(r["sigma"]),
                        "correct": int(r["is_correct"]),
                        "action": (r.get("action") or "").strip().upper(),
                    }
                )
            except (TypeError, ValueError, KeyError):
                continue
    return rows


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "csv",
        nargs="?",
        type=Path,
        default=DEFAULT_CSV,
        help="graded_results.csv path",
    )
    ap.add_argument(
        "--model",
        default=os.environ.get("COS_PUBTABLE_MODEL", "(set COS_PUBTABLE_MODEL)"),
    )
    ap.add_argument(
        "--hardware",
        default=os.environ.get("COS_PUBTABLE_HARDWARE", "(set COS_PUBTABLE_HARDWARE)"),
    )
    args = ap.parse_args()
    path: Path = args.csv
    if not path.is_file():
        print(f"error: missing {path}", file=sys.stderr)
        return 1

    rows = load_rows(path)
    n = len(rows)
    if n == 0:
        print("error: no valid rows", file=sys.stderr)
        return 1

    correct = sum(r["correct"] for r in rows)
    accepted = [r for r in rows if r["action"] != "ABSTAIN"]
    abstained = n - len(accepted)

    c_sigs = [r["sigma"] for r in rows if r["correct"]]
    w_sigs = [
        r["sigma"]
        for r in rows
        if not r["correct"] and r["action"] != "ABSTAIN"
    ]
    conc = sum(1 for c in c_sigs for w in w_sigs if c < w)
    ties = sum(0.5 for c in c_sigs for w in w_sigs if c == w)
    denom = len(c_sigs) * len(w_sigs)
    auroc = (conc + ties) / denom if c_sigs and w_sigs else 0.5

    brier = sum((1.0 - r["sigma"] - r["correct"]) ** 2 for r in rows) / n

    bins: list[list[dict]] = [[] for _ in range(10)]
    for r in rows:
        b = min(int((1.0 - r["sigma"]) * 10), 9)
        bins[b].append(r)
    ece = 0.0
    for b in bins:
        if not b:
            continue
        conf = sum(1.0 - r["sigma"] for r in b) / len(b)
        accb = sum(r["correct"] for r in b) / len(b)
        ece += abs(conf - accb) * (len(b) / n)

    wrong_conf = sum(1 for r in rows if r["correct"] == 0 and r["sigma"] < 0.3)

    acc_all = correct / n
    acc_acc = (
        sum(r["correct"] for r in accepted) / len(accepted) if accepted else 0.0
    )
    abst_rate = abstained / n

    print(
        f"""
╔══════════════════════════════════════════════════════════╗
║  CREATION OS — DEFINITIVE SELECTIVE PREDICTION RESULTS  ║
╠══════════════════════════════════════════════════════════╣
║  Model: {args.model:<43} ║
║  Hardware: {args.hardware:<40} ║
╠══════════════════════════════════════════════════════════╣
║  Metric                          │  Value               ║
╠──────────────────────────────────┼───────────────────────╣
║  Prompts                         │  {n:>6}              ║
║  Accuracy (all)                  │  {acc_all:>6.1%}              ║
║  Accuracy (σ-accepted)           │  {acc_acc:>6.1%}              ║
║  Abstention rate                 │  {abst_rate:>6.1%}              ║
║  AUROC                           │  {auroc:>6.3f}              ║
║  ECE                             │  {ece:>6.4f}             ║
║  Brier Score                     │  {brier:>6.4f}             ║
║  Wrong + confident (σ<0.3)       │  {wrong_conf:>6}              ║
║  Profile                         │  SELECTIVE SENSITIVITY ║
╠══════════════════════════════════╧═══════════════════════╣
║  AUROC={auroc:.3f} — report numbers from this CSV only.              ║
╚══════════════════════════════════════════════════════════╝
"""
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
