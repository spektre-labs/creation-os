#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""BENCH-5: run ``cos-evolve omega`` and archive stdout to JSON.

This is the bounded substitute for an wall-clock “overnight” job: it runs
the recursive Ω loop on the shipped tiny fixture with enough inner work
that accept/revert counters move, then writes ``benchmarks/evolve/overnight_run.json``.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
import time
from pathlib import Path


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[2])
    ap.add_argument("--fixture", default="benchmarks/evolve/fixture_tiny.jsonl")
    ap.add_argument("--rounds", type=int, default=20)
    ap.add_argument("--gens", type=int, default=250)
    args = ap.parse_args()
    repo: Path = args.repo
    bin_path = repo / "cos-evolve"
    if not bin_path.is_file():
        print(f"record_overnight_omega: missing {bin_path}", file=sys.stderr)
        return 2

    ev_home = Path(os.environ.get("COS_EVOLVE_HOME", "/tmp/cos_overnight_evolve"))
    ev_home.mkdir(parents=True, exist_ok=True)

    cmd = [
        str(bin_path),
        "omega",
        "--fixture",
        str(repo / args.fixture),
        "--rounds",
        str(args.rounds),
        "--gens",
        str(args.gens),
    ]
    env = os.environ.copy()
    env["COS_EVOLVE_HOME"] = str(ev_home)

    t0 = time.monotonic()
    proc = subprocess.run(
        cmd,
        cwd=str(repo),
        env=env,
        stdin=subprocess.DEVNULL,
        capture_output=True,
        text=True,
    )
    elapsed = time.monotonic() - t0
    merged = (proc.stdout or "") + ("\n" + proc.stderr if proc.stderr else "")

    brier_delta_m = re.search(
        r"cos-omega: brier ([0-9.]+) → ([0-9.]+)\s+Δ=([+-][0-9.]+)\s+\(\+(\d+) accepted / -(\d+) reverted\)",
        merged,
    )
    start_m = re.search(r"round 0: brier=([0-9.]+)", merged)

    doc = {
        "schema": "cos.evolve.overnight_run.v1",
        "command": cmd,
        "cwd": str(repo),
        "cos_evolve_home": str(ev_home),
        "exit_code": proc.returncode,
        "elapsed_s": round(elapsed, 3),
        "parsed": {
            "brier_start": float(start_m.group(1)) if start_m else None,
            "brier_end": float(brier_delta_m.group(2)) if brier_delta_m else None,
            "brier_delta": float(brier_delta_m.group(3)) if brier_delta_m else None,
            "mutations_accepted": int(brier_delta_m.group(4)) if brier_delta_m else None,
            "mutations_reverted": int(brier_delta_m.group(5)) if brier_delta_m else None,
        },
        "stdout_tail": merged[-12000:],
    }

    out = repo / "benchmarks" / "evolve" / "overnight_run.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(doc, indent=2) + "\n", encoding="utf-8")
    print(out)
    return 0 if proc.returncode == 0 else proc.returncode


if __name__ == "__main__":
    raise SystemExit(main())
