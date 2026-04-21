#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""BENCH-3: merge per-dataset ``cos-coverage-curve`` JSON into one file.

Requires ``./creation_os_sigma_coverage_curve`` (``make cos-coverage-curve``).

Writes ``benchmarks/suite/coverage_curves.json`` with schema
``cos.suite_coverage_curves.v1``.
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path
from tempfile import NamedTemporaryFile
from typing import Any, Dict, List, Tuple


def run_curve(
    bin_path: Path, repo: Path, dataset_path: Path, step: float, delta: float
) -> Dict[str, Any]:
    with NamedTemporaryFile(prefix="cc_", suffix=".json", delete=False) as tf:
        out = Path(tf.name)
    try:
        subprocess.run(
            [
                str(bin_path),
                "--dataset",
                str(dataset_path),
                "--mode",
                "pipeline",
                "--step",
                str(step),
                "--delta",
                str(delta),
                "--out",
                str(out),
            ],
            cwd=str(repo),
            check=True,
            capture_output=True,
            text=True,
        )
        return json.loads(out.read_text(encoding="utf-8"))
    finally:
        out.unlink(missing_ok=True)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[2])
    ap.add_argument("--step", type=float, default=0.05)
    ap.add_argument("--delta", type=float, default=0.10)
    ap.add_argument(
        "--strict",
        action="store_true",
        help="fail if any expected detail JSONL is missing",
    )
    args = ap.parse_args()
    repo: Path = args.repo
    bin_path = repo / "creation_os_sigma_coverage_curve"
    if not bin_path.is_file() or not os.access(bin_path, os.X_OK):
        print(f"build_suite_coverage_curves: build {bin_path} first", file=sys.stderr)
        return 2

    sets: List[Tuple[str, Path]] = [
        ("truthfulqa", repo / "benchmarks/pipeline/truthfulqa_817_detail.jsonl"),
        ("arc_challenge", repo / "benchmarks/suite/arc_challenge_detail.jsonl"),
        ("arc_easy", repo / "benchmarks/suite/arc_easy_detail.jsonl"),
        ("gsm8k", repo / "benchmarks/suite/gsm8k_detail.jsonl"),
        ("hellaswag", repo / "benchmarks/suite/hellaswag_detail.jsonl"),
    ]

    out_doc: Dict[str, Any] = {
        "schema": "cos.suite_coverage_curves.v1",
        "step": args.step,
        "delta": args.delta,
        "datasets": {},
    }
    for name, path in sets:
        if not path.is_file():
            msg = f"missing detail JSONL: {path}"
            if args.strict:
                print(f"build_suite_coverage_curves: {msg}", file=sys.stderr)
                return 3
            print(f"skip ({msg})", file=sys.stderr)
            continue
        out_doc["datasets"][name] = run_curve(
            bin_path, repo, path, args.step, args.delta
        )

    dest = repo / "benchmarks/suite/coverage_curves.json"
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_text(json.dumps(out_doc, indent=2) + "\n", encoding="utf-8")
    print(dest)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
