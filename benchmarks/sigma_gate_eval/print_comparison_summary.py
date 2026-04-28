#!/usr/bin/env python3
"""Print holdout summary lines for Makefile / docs (see sigma-comparison-table)."""
from __future__ import annotations

import json
import sys
from pathlib import Path


def main() -> None:
    root = Path(__file__).resolve().parents[2]
    hp = root / "benchmarks" / "sigma_gate_eval" / "results_holdout" / "holdout_summary.json"
    print("Full table + disclaimers: docs/sigma_gate_v4_comparison_table.md")
    if not hp.is_file():
        print("No holdout_summary.json yet — run: make sigma-holdout", file=sys.stderr)
        sys.exit(0)
    h = json.loads(hp.read_text(encoding="utf-8"))
    print("training_cv_auroc_manifest:", h.get("training_cv_auroc_manifest"))
    print("holdout_auroc_wrong_vs_sigma:", h.get("auroc_wrong_vs_sigma"))
    print("sigma_gap_mean_wrong_minus_correct:", h.get("sigma_gap_mean_wrong_minus_correct"))
    print("wrong_confident_accept:", h.get("wrong_confident_accept"))


if __name__ == "__main__":
    main()
