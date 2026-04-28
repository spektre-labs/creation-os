#!/usr/bin/env python3
"""Print holdout + cross-domain summary and a coarse publication-readiness verdict."""
from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path


def _f(x: object) -> float:
    try:
        v = float(x)  # type: ignore[arg-type]
    except (TypeError, ValueError):
        return float("nan")
    return v


def _fmt(x: float) -> str:
    return f"{x:.4f}" if not math.isnan(x) else "nan"


def _pending(path: Path) -> bool:
    return not path.is_file()


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--full",
        action="store_true",
        help="Print ASCII verdict box (for logs after full pipeline).",
    )
    args = ap.parse_args()

    root = Path(__file__).resolve().parents[2]
    h_path = root / "benchmarks/sigma_gate_eval/results_holdout/holdout_summary.json"
    c_path = root / "benchmarks/sigma_gate_eval/results_cross_domain/cross_domain_summary.json"

    cv = 0.9428
    h_auroc = float("nan")
    t_auroc = float("nan")
    e_auroc = float("nan")

    if not _pending(h_path):
        hdoc = json.loads(h_path.read_text(encoding="utf-8"))
        h_auroc = _f(hdoc.get("auroc_wrong_vs_sigma", float("nan")))
        cv_m = _f(hdoc.get("training_cv_auroc_manifest", float("nan")))
        if not math.isnan(cv_m):
            cv = cv_m

    if not _pending(c_path):
        cdoc = json.loads(c_path.read_text(encoding="utf-8"))
        t_auroc = _f(cdoc.get("triviaqa_auroc_wrong_vs_sigma", float("nan")))
        e_auroc = _f(cdoc.get("halueval_auroc_wrong_vs_sigma", float("nan")))

    print("=== sigma-gate v4 results (JSON artefacts) ===")
    print(f"  TruthfulQA CV (manifest / holdout ref): {_fmt(cv)}")
    print(f"  Holdout AUROC (wrong vs sigma):         {_fmt(h_auroc) if not _pending(h_path) else 'pending'}")
    print(f"  TriviaQA AUROC (cross, smoke harness):   {_fmt(t_auroc) if not _pending(c_path) else 'pending'}")
    print(f"  HaluEval AUROC (cross, smoke harness):   {_fmt(e_auroc) if not _pending(c_path) else 'pending'}")

    if not args.full:
        return

    if _pending(h_path) or _pending(c_path):
        print("\n  Verdict: incomplete (run holdout + cross-domain pipelines).")
        return

    h_ok = not math.isnan(h_auroc) and h_auroc >= 0.85
    t_ok = not math.isnan(t_auroc) and t_auroc >= 0.70
    e_ok = not math.isnan(e_auroc) and e_auroc >= 0.70

    print()
    print("  +------------------------------------------+")
    print(f"  |  CV AUROC (training ref):    {_fmt(cv):>8}   |")
    print(f"  |  Holdout AUROC:               {_fmt(h_auroc):>8}   |")
    print(f"  |  TriviaQA AUROC (unseen):     {_fmt(t_auroc):>8}   |")
    print(f"  |  HaluEval AUROC (unseen):     {_fmt(e_auroc):>8}   |")
    print("  +------------------------------------------+")
    print()

    if h_ok and t_ok and e_ok:
        print("  FULL VALIDATION (threshold smoke): holdout >= 0.85 and both cross >= 0.70")
        print("  Still: cross AUROCs are not comparable to TruthfulQA CV (different labels).")
    elif h_ok and (not t_ok or not e_ok):
        print("  Holdout strong; cross-domain below 0.70 on one or both smokes — caveat generalization.")
    elif not math.isnan(h_auroc) and h_auroc >= 0.75:
        print("  Promising holdout but review thresholds / label noise before external claims.")
    else:
        print("  Holdout or cross metrics weak or missing — investigate before publication.")


if __name__ == "__main__":
    main()
    sys.exit(0)
