#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""Print metacognitive profile from benchmarks/graded/graded_results.csv."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

_SCRIPT_DIR = Path(__file__).resolve().parent
if str(_SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(_SCRIPT_DIR))

from metacognitive_lib import classify_profile, load_graded_rows, wrong_confident_low_sigma

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_CSV = ROOT / "benchmarks" / "graded" / "graded_results.csv"


def main() -> int:
    ap = argparse.ArgumentParser(description="Metacognitive profile from graded results")
    ap.add_argument(
        "csv",
        nargs="?",
        type=Path,
        default=DEFAULT_CSV,
        help="graded_results.csv path",
    )
    ap.add_argument("--json", action="store_true", help="Emit one JSON line to stdout")
    args = ap.parse_args()
    path: Path = args.csv
    if not path.is_file():
        print(f"error: missing {path}", file=sys.stderr)
        return 1

    rows = load_graded_rows(path)
    if len(rows) < 10:
        print(f"Too few numeric-σ rows ({len(rows)}); need ≥10 for stable profile.")
        return 1

    pr = classify_profile(rows)
    wc = wrong_confident_low_sigma(rows, 0.3)

    if args.json:
        payload = {
            "profile": pr.profile,
            "description": pr.description,
            "mean_sigma": pr.mean_sigma,
            "abstain_rate": pr.abstain_rate,
            "auroc": pr.auroc,
            "auroc_note": pr.auroc_note,
            "acc_all": pr.acc_all,
            "acc_accepted": pr.acc_accepted,
            "control_gain": pr.control_gain,
            "n_rows": pr.n_rows,
            "wrong_confident_sigma_lt_0_3": wc,
        }
        print(json.dumps(payload, sort_keys=True))
        return 0

    print("=== METACOGNITIVE PROFILE (graded) ===")
    print(f"Rows (numeric σ): {pr.n_rows}")
    print(f"Mean σ:           {pr.mean_sigma:.4f}")
    print(f"Abstain rate:     {pr.abstain_rate:.1%}")
    print(f"AUROC:            {pr.auroc_note}")
    print(f"Wrong + σ<0.3:   {wc}")
    print()
    print(f"Profile: {pr.profile}")
    print(f"  {pr.description}")
    print()
    print(f"Monitoring (AUROC):     {pr.auroc if pr.auroc is not None else float('nan'):.4f}")
    print(f"Control (acc lift):     {pr.control_gain:+.1%}")
    print(f"  Accuracy all → accepted: {pr.acc_all:.1%} → {pr.acc_accepted:.1%}")

    if pr.auroc is not None and pr.auroc > 0.6 and pr.control_gain > 0.05:
        print("\nBoth monitoring and control look effective on this run.")
    elif pr.auroc is not None and pr.auroc > 0.6:
        print("\nMonitoring is decent; control gain is modest (check τ / abstention).")
    else:
        print("\nMonitoring is weak or undefined; do not over-claim selective sensitivity.")

    return 0


if __name__ == "__main__":
    sys.exit(main())
