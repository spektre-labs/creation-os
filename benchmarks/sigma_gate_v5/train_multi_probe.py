#!/usr/bin/env python3
"""Train one LSD σ-gate bundle on the full multi-domain CSV (optional baseline)."""
from __future__ import annotations

import argparse
import csv
import os
import subprocess
import sys
from pathlib import Path


def _repo() -> Path:
    return Path(__file__).resolve().parents[2]


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--epochs", type=int, default=12)
    ap.add_argument("--min-pairs", type=int, default=400)
    args = ap.parse_args()
    repo = _repo()
    csv_path = repo / "benchmarks/sigma_gate_v5/intermediate/multi_contrastive.csv"
    if not csv_path.is_file():
        print(f"Missing {csv_path}", file=sys.stderr)
        sys.exit(2)
    out = repo / "benchmarks/sigma_gate_v5/results/v5_multidomain"
    out.mkdir(parents=True, exist_ok=True)

    with csv_path.open(encoding="utf-8", newline="") as fp:
        ntrain = sum(1 for _ in csv.DictReader(fp))
    env = os.environ.copy()
    env["PYTHONPATH"] = str(repo / "python")

    cmd = [
        sys.executable,
        str(repo / "benchmarks/sigma_gate_lsd/adapt_lsd.py"),
        "--prompts",
        str(csv_path),
        "--output",
        str(out),
        "--limit",
        str(ntrain),
        "--epochs",
        str(int(args.epochs)),
        "--min-pairs",
        str(int(args.min_pairs)),
    ]
    print("Running:", " ".join(cmd))
    subprocess.run(cmd, check=True, cwd=str(repo), env=env)
    print(f"OK -> {out / 'sigma_gate_lsd.pkl'}")


if __name__ == "__main__":
    main()
