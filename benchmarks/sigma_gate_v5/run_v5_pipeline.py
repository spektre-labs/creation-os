#!/usr/bin/env python3
"""Orchestrate σ-gate v5 multi-dataset lab pipeline."""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path


def _repo() -> Path:
    return Path(__file__).resolve().parents[2]


def _run(script: str) -> None:
    repo = _repo()
    p = repo / "benchmarks" / "sigma_gate_v5" / script
    print(f"\n>>> {p.name}")
    subprocess.run([sys.executable, str(p)], check=True, cwd=str(repo))


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--skip-full-train",
        action="store_true",
        help="Skip train_multi_probe.py (full multi-domain bundle).",
    )
    args = ap.parse_args()

    print("=== sigma-gate v5: multi-dataset + semantic labels + PRISM (lab) ===")
    _run("prepare_multi_dataset.py")
    _run("generate_and_label.py")
    _run("create_contrastive_pairs.py")
    if not args.skip_full_train:
        _run("train_multi_probe.py")
    _run("eval_cross_domain.py")

    summ = _repo() / "benchmarks/sigma_gate_v5/results/v5_summary.json"
    if summ.is_file():
        doc = json.loads(summ.read_text(encoding="utf-8"))
        print(f"\n{'=' * 60}")
        print("  sigma-gate v5 leave-one-source-out (AUROC wrong vs sigma)")
        print(f"{'=' * 60}")
        for k, v in sorted(doc.get("per_dataset_auroc", {}).items()):
            print(f"  {k:22s}  {v:.4f}" if isinstance(v, (int, float)) and v == v else f"  {k:22s}  {v}")
        m = doc.get("mean_auroc", 0)
        print(f"  {'mean':22s}  {m:.4f}" if isinstance(m, (int, float)) and m == m else f"  {'mean':22s}  {m}")
        print(f"{'=' * 60}\n")


if __name__ == "__main__":
    main()
