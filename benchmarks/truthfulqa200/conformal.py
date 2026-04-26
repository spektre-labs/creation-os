#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""Conformal-style τ selection from TruthfulQA-200 results.csv (split calibration / test)."""

from __future__ import annotations

import argparse
import csv
import random
import sys
from pathlib import Path
from typing import Any, Dict, List, Tuple

REPO_ROOT = Path(__file__).resolve().parents[2]
BENCH_DIR = Path(__file__).resolve().parent

# Import grading helpers from sibling module
sys.path.insert(0, str(BENCH_DIR))
import grade as gq  # type: ignore  # noqa: E402


def load_joined(
    results_path: Path, prompts_path: Path
) -> List[Dict[str, Any]]:
    prompts: Dict[str, Dict[str, str]] = {}
    with prompts_path.open(newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            prompts[row["id"]] = row
    rows = []
    with results_path.open(newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            pid = row["id"]
            pr = prompts.get(pid, {})
            action = (row.get("action") or "").strip()
            sig = float(row["sigma"])
            resp = row.get("response", "")
            if action == "ABSTAIN":
                continue
            c = gq.grade_one(
                resp, pr.get("best_answer", ""), pr.get("best_incorrect_answer", "")
            )
            rows.append({"id": pid, "sigma": sig, "correct": c})
    return rows


def pick_tau(calib: List[Dict[str, Any]], target: float) -> Tuple[float, float, int]:
    """Largest acceptance set (sigma < tau) with mean(correct) >= target."""
    if not calib:
        return float("nan"), 0.0, 0
    sigs = sorted({float(r["sigma"]) for r in calib})
    cands = list(sigs) + [sigs[-1] + 0.25]
    best: Tuple[int, float] | None = None  # (count, tau)
    best_tau = float("nan")
    for tau in cands:
        sub = [r for r in calib if float(r["sigma"]) < tau]
        if not sub:
            continue
        acc = sum(int(r["correct"]) for r in sub) / len(sub)
        if acc + 1e-9 >= target:
            key = (len(sub), tau)
            if best is None or key > best:
                best = key
                best_tau = tau
    if best is None:
        return float("nan"), 0.0, 0
    sub = [r for r in calib if float(r["sigma"]) < best_tau]
    acc = sum(int(r["correct"]) for r in sub) / len(sub)
    return best_tau, acc, len(sub)


def main() -> int:
    try:
        from sklearn.model_selection import train_test_split
    except ImportError:
        print("error: pip install scikit-learn", file=sys.stderr)
        return 2

    ap = argparse.ArgumentParser()
    ap.add_argument("--results", default=str(BENCH_DIR / "results.csv"))
    ap.add_argument("--prompts", default=str(BENCH_DIR / "prompts.csv"))
    ap.add_argument("--seed", type=int, default=42)
    ap.add_argument(
        "--append-results-md",
        default=None,
        help="Append ## Conformal Guarantees section to this markdown file",
    )
    ap.add_argument(
        "--plot",
        default=str(BENCH_DIR / "accuracy_coverage.png"),
        help="PNG path (requires matplotlib)",
    )
    args = ap.parse_args()

    rows = load_joined(Path(args.results), Path(args.prompts))
    if len(rows) < 20:
        print(f"error: too few graded rows ({len(rows)}); need results + prompts", file=sys.stderr)
        return 2

    y = [int(r["correct"]) for r in rows]
    calib, test = train_test_split(
        rows,
        test_size=0.5,
        random_state=args.seed,
        stratify=y,
        shuffle=True,
    )

    targets = [0.80, 0.85, 0.90, 0.95]
    lines = [
        "## Conformal guarantees (split calibration)",
        "",
        "Random 50/50 split stratified by **correct** (0/1), seed=%d. "
        "Rule: **ACCEPT** iff σ < τ (calibration set picks τ). "
        "This is an **empirical** split, not a full exchangeability proof — "
        "see Angelopoulos & Bates (2023); Gui et al. (2024) for references."
        % args.seed,
        "",
        "| Target acc (cal) | τ | Empirical acc (cal, σ<τ) | Coverage (cal) | "
        "Acc (test, σ<τ) | Coverage (test) |",
        "|---:|---:|---:|---:|---:|---:|",
    ]
    plot_pts: List[Tuple[float, float]] = []
    for tgt in targets:
        tau, acc_c, n_c = pick_tau(calib, tgt)
        if n_c == 0:
            lines.append(f"| {100*tgt:.0f}% | — | — | — | — | — |")
            continue
        cov_c = n_c / len(calib)
        sub_t = [r for r in test if float(r["sigma"]) < tau]
        acc_t = (
            sum(int(r["correct"]) for r in sub_t) / len(sub_t) if sub_t else float("nan")
        )
        cov_t = len(sub_t) / len(test) if test else 0.0
        lines.append(
            f"| {100*tgt:.0f}% | {tau:.4f} | {100*acc_c:.1f}% | {100*cov_c:.1f}% | "
            f"{100*acc_t:.1f}% | {100*cov_t:.1f}% |"
        )
        plot_pts.append((cov_t, acc_t))

    lines.append("")

    plot_path = Path(args.plot)
    try:
        import matplotlib.pyplot as plt  # type: ignore

        if plot_pts:
            xs = [p[0] for p in plot_pts]
            ys = [p[1] for p in plot_pts]
            plt.figure(figsize=(5, 3))
            plt.plot(xs, ys, "o-")
            plt.xlabel("Coverage (test)")
            plt.ylabel("Accuracy (test, σ<τ)")
            plt.ylim(0.0, 1.05)
            plt.xlim(0.0, 1.05)
            plt.grid(True, alpha=0.3)
            plt.tight_layout()
            plt.savefig(plot_path, dpi=120)
            plt.close()
            lines.append(f"![accuracy vs coverage]({plot_path.name})")
            lines.append("")
    except ImportError:
        lines.append("*Figure skipped: install `matplotlib` to emit accuracy–coverage PNG.*")
        lines.append("")

    block = "\n".join(lines) + "\n"
    print(block)
    if args.append_results_md:
        p = Path(args.append_results_md)
        prev = p.read_text(encoding="utf-8") if p.is_file() else ""
        p.write_text(prev.rstrip() + "\n\n" + block, encoding="utf-8")
        print(f"appended → {p}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
