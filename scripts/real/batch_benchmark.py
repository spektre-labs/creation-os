#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""Parallel graded prompts against cos serve /v1/gate (ThreadPoolExecutor)."""

from __future__ import annotations

import argparse
import csv
import json
import sys
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

import requests

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_CSV = ROOT / "benchmarks" / "graded" / "graded_prompts.csv"


def one_gate(url: str, prompt: str, timeout: float) -> dict:
    r = requests.post(
        f"{url.rstrip('/')}/v1/gate",
        json={"prompt": prompt},
        timeout=timeout,
    )
    r.raise_for_status()
    return r.json()


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--url",
        default="http://127.0.0.1:3001",
        help="cos serve base URL",
    )
    ap.add_argument(
        "--csv",
        type=Path,
        default=DEFAULT_CSV,
        help="graded_prompts.csv path",
    )
    ap.add_argument("--workers", type=int, default=4, help="concurrent HTTP workers")
    ap.add_argument("--timeout", type=float, default=120.0, help="per-request timeout s")
    ap.add_argument("--out", type=Path, help="optional JSONL of raw responses")
    args = ap.parse_args()

    if not args.csv.is_file():
        print(f"batch_benchmark: missing {args.csv}", file=sys.stderr)
        return 1

    rows = list(csv.DictReader(args.csv.open(newline="", encoding="utf-8")))
    prompts = [(i, r["prompt"]) for i, r in enumerate(rows)]

    out_f = args.out.open("w", encoding="utf-8") if args.out else None

    def work(item: tuple[int, str]) -> tuple[int, str, dict | None, str | None]:
        idx, pr = item
        try:
            js = one_gate(args.url, pr, args.timeout)
            if out_f:
                out_f.write(json.dumps({"i": idx, "prompt": pr, "resp": js}) + "\n")
            return idx, pr, js, None
        except Exception as e:  # noqa: BLE001
            return idx, pr, None, str(e)

    results: dict[int, tuple[str, dict | None, str | None]] = {}
    with ThreadPoolExecutor(max_workers=max(1, args.workers)) as pool:
        futs = {pool.submit(work, p): p for p in prompts}
        for fut in as_completed(futs):
            idx, pr, js, err = fut.result()
            results[idx] = (pr, js, err)

    n_ok = 0
    for i in range(len(prompts)):
        pr, js, err = results.get(i, ("", None, "missing"))
        if err:
            print(f"  [{i}] FAIL {repr(err)[:120]}", flush=True)
        else:
            n_ok += 1
            sig = js.get("sigma") if isinstance(js, dict) else None
            print(f"  [{i}] OK σ={sig}", flush=True)
    if out_f:
        out_f.close()
    print(f"batch_benchmark: {n_ok}/{len(prompts)} requests OK → {args.url}")
    return 0 if n_ok == len(prompts) else 1


if __name__ == "__main__":
    raise SystemExit(main())
