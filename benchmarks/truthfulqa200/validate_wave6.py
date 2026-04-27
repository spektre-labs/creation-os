#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""Sanity checks for full semantic-σ TruthfulQA results (see Wave 6 checklist)."""

from __future__ import annotations

import csv
import sys
from pathlib import Path


def main() -> int:
    ap = Path(
        __file__
    ).resolve().parent / "results.csv" if len(sys.argv) < 2 else Path(sys.argv[1])
    if not ap.is_file():
        print(f"error: missing {ap}", file=sys.stderr)
        return 2
    with ap.open(newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    sigmas = [float(r["sigma"]) for r in rows if r.get("sigma")]
    actions = [r.get("action", "") for r in rows]
    unique_s = len(set(round(s, 3) for s in sigmas))
    abstains = sum(1 for a in actions if a == "ABSTAIN")
    accepts = sum(1 for a in actions if a == "ACCEPT")
    rethinks = sum(1 for a in actions if a == "RETHINK")
    n = len(rows)
    srange = max(sigmas) - min(sigmas) if sigmas else 0.0
    print(f"N={n}")
    print(f"Unique σ (rounded 3dp): {unique_s}")
    print(f"σ range: {min(sigmas):.3f} - {max(sigmas):.3f}  (span {srange:.3f})")
    print(f"ACCEPT={accepts} RETHINK={rethinks} ABSTAIN={abstains}")
    errs = []
    if n < 100:
        errs.append(f"need at least 100 rows for paper checklist, got {n}")
    if unique_s <= 20:
        errs.append(f"low σ diversity: only {unique_s} unique values (expect > 20)")
    if srange <= 0.3 + 1e-9:
        errs.append(
            f"σ span {srange:.3f} <= 0.3 — likely logprob-only or compressed path"
        )
    if abstains <= 5:
        errs.append(f"only {abstains} ABSTAIN (expect > 5 for a real gate sweep)")
    if errs:
        print("VALIDATION FAILED:", file=sys.stderr)
        for e in errs:
            print(f"  - {e}", file=sys.stderr)
        return 1
    print("VALIDATION PASSED — plausibility checks for full semantic-entropy data")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
