#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""AUROC, AURC, risk–coverage curve, and comparison table from graded_results.csv.

Run after `graded_benchmark.py` / `graded_benchmark.sh`.
Reads:  benchmarks/graded/graded_results.csv
Writes: benchmarks/graded/risk_coverage.csv
Appends: benchmarks/graded/graded_comparison.md (if present)
"""
from __future__ import annotations

import argparse
import csv
import math
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_CSV = ROOT / "benchmarks" / "graded" / "graded_results.csv"
RISK_CSV = ROOT / "benchmarks" / "graded" / "risk_coverage.csv"
COMPARISON_MD = ROOT / "benchmarks" / "graded" / "graded_comparison.md"


def load_rows(path: Path) -> list[dict]:
    rows: list[dict] = []
    with path.open(newline="", encoding="utf-8") as f:
        for r in csv.DictReader(f):
            try:
                rows.append(
                    {
                        "sigma": float(r["sigma"]),
                        "correct": int(r["is_correct"]),
                        "action": (r.get("action") or "N/A").strip().upper(),
                    }
                )
            except (TypeError, ValueError, KeyError):
                continue
    return rows


def auroc_value(rows: list[dict]) -> tuple[float | None, int, int, str]:
    """Mann–Whitney U style: P(random correct has lower σ than incorrect)."""
    correct = [r["sigma"] for r in rows if r["correct"] == 1]
    incorrect = [r["sigma"] for r in rows if r["correct"] == 0]
    if not incorrect or not correct:
        return None, len(correct), len(incorrect), "all_correct_or_all_incorrect"
    concordant = 0.0
    total = 0
    for c in correct:
        for i in incorrect:
            total += 1
            if c < i:
                concordant += 1.0
            elif c == i:
                concordant += 0.5
    return concordant / total, len(correct), len(incorrect), "ok"


def risk_coverage_series(rows_sorted: list[dict]) -> tuple[list[float], list[float], list[float]]:
    """rows_sorted: ascending σ (most confident first). Risk at k = error rate in prefix of size k."""
    n = len(rows_sorted)
    risks: list[float] = []
    coverages: list[float] = []
    accs: list[float] = []
    errors_so_far = 0
    for k in range(1, n + 1):
        if rows_sorted[k - 1]["correct"] == 0:
            errors_so_far += 1
        risk = errors_so_far / k
        coverage = k / n
        risks.append(risk)
        coverages.append(coverage)
        accs.append(1.0 - risk)
    return risks, coverages, accs


def trapezoid_aurc(coverages: list[float], risks: list[float]) -> float:
    if len(risks) < 2:
        return 0.0
    aurc = 0.0
    for k in range(1, len(risks)):
        aurc += 0.5 * (risks[k] + risks[k - 1]) * (coverages[k] - coverages[k - 1])
    return aurc


def print_risk_table(rows_sorted: list[dict], n: int) -> None:
    print("=== RISK-COVERAGE TABLE ===")
    print(f"{'Coverage':>10} {'Risk':>10} {'Acc':>10}")
    for target_cov in (0.5, 0.6, 0.7, 0.8, 0.9, 1.0):
        k = max(1, int(math.ceil(target_cov * n)))
        subset = rows_sorted[:k]
        err = sum(1 for r in subset if r["correct"] == 0)
        risk = err / k
        acc = 1.0 - risk
        print(f"{target_cov:>10.0%} {risk:>10.3f} {acc:>10.1%}")


def print_ascii_curve(rc_rows: list[dict]) -> None:
    """rc_rows: dicts with 'cov' and 'acc' from risk_coverage.csv-like data."""
    if not rc_rows:
        return
    print("\n=== RISK-COVERAGE CURVE (accuracy vs coverage) ===")
    for acc_line in range(100, -1, -10):
        line = f"{acc_line:3d}% ┤"
        for i in range(20):
            cov_target = (i + 0.5) / 20.0
            closest = min(rc_rows, key=lambda r: abs(r["cov"] - cov_target))
            if closest["acc"] * 100.0 >= float(acc_line):
                line += "█"
            else:
                line += " "
        print(line)
    print("     └" + "─" * 20)
    print("      0%        50%       100%")
    print("              coverage")
    print("")
    print("Ideal: flat at 100% across all coverage (never wrong).")
    print("Random: diagonal from top-left to bottom-right.")
    print("σ-gate: compare to your run’s empirical curve (honest shape).")


def print_comparison_table(rows: list[dict]) -> None:
    n = len(rows)
    total_correct = sum(r["correct"] for r in rows)
    total_acc = total_correct / n if n else 0.0

    accepted = [r for r in rows if r["action"] != "ABSTAIN"]
    abstained = [r for r in rows if r["action"] == "ABSTAIN"]
    acc_accepted = (
        sum(r["correct"] for r in accepted) / len(accepted) if accepted else 0.0
    )
    wrong_confident = sum(
        1
        for r in accepted
        if r["correct"] == 0 and r["sigma"] < 0.3
    )

    print("")
    print("=== COMPARISON TABLE (same run; ‘With σ’ = observed routing) ===")
    print(f"{'Metric':<35} {'Without σ':>12} {'With σ':>12}")
    print(f"{'─'*35} {'─'*12} {'─'*12}")
    print(f"{'Total prompts':<35} {n:>12} {n:>12}")
    print(f"{'Answered':<35} {n:>12} {len(accepted):>12}")
    print(f"{'Abstained':<35} {0:>12} {len(abstained):>12}")
    print(f"{'Correct (all)':<35} {total_correct:>12} {total_correct:>12}")
    print(f"{'Accuracy (all)':<35} {total_acc:>11.1%} {total_acc:>11.1%}")
    print(f"{'Accuracy (answered only)':<35} {'—':>12} {acc_accepted:>11.1%}")
    lift = acc_accepted - total_acc
    print(f"{'Accuracy lift (answered − all)':<35} {'—':>12} {lift:>+11.1%}")
    print(
        f"{'Wrong + confident (σ<0.3, answered)':<35} {'—':>12} {wrong_confident:>12}"
    )
    print("")
    if acc_accepted > total_acc:
        print("Answered-only accuracy exceeds all-rows accuracy (abstain text "
              "often fails strict grader) ✓")
    if wrong_confident == 0:
        print("ZERO wrong + low-σ among answered rows (σ<0.3) ✓")
    else:
        print(
            f"Wrong + low-σ (σ<0.3) among answered: {wrong_confident} "
            "(report honestly)."
        )


def append_markdown(
    path: Path,
    text: str,
) -> None:
    if not path.is_file():
        return
    with path.open("a", encoding="utf-8") as f:
        f.write("\n\n")
        f.write(text)


def main() -> int:
    ap = argparse.ArgumentParser(description="AUROC / AURC from graded_results.csv")
    ap.add_argument(
        "--csv",
        type=Path,
        default=DEFAULT_CSV,
        help="Path to graded_results.csv",
    )
    ap.add_argument("--no-append-md", action="store_true", help="Do not append to graded_comparison.md")
    args = ap.parse_args()
    path: Path = args.csv
    if not path.is_file():
        print(f"error: missing {path}", file=sys.stderr)
        return 1

    rows = load_rows(path)
    if len(rows) < 4:
        print(f"Too few numeric-σ rows for AUROC ({len(rows)}); need ≥4.")
        return 0

    rows.sort(key=lambda r: r["sigma"])
    n = len(rows)

    auroc, n_cor, n_inc, reason = auroc_value(rows)
    risks, coverages, accs = risk_coverage_series(rows)
    aurc = trapezoid_aurc(coverages, risks)

    print("=== AUROC / AURC RESULTS ===")
    if auroc is None:
        print(f"AUROC: undefined ({reason}; correct={n_cor}, incorrect={n_inc})")
    else:
        print(f"AUROC: {auroc:.3f}")
        print("  (1.0 = perfect rank; 0.5 = random; <0.5 = inverted vs this convention)")
    print(f"AURC:  {aurc:.4f}")
    print("  (integrated risk vs coverage; lower = better under this curve)")
    print("")
    print(f"Correct: {n_cor} | Incorrect: {n_inc}")
    print("")

    print_risk_table(rows, n)

    h = n // 2
    top_half = rows[:h] if h else []
    bot_half = rows[h:] if h < n else []
    acc_top = sum(r["correct"] for r in top_half) / len(top_half) if top_half else 0.0
    acc_bot = sum(r["correct"] for r in bot_half) / len(bot_half) if bot_half else 0.0
    print("")
    print("=== THE PROOF (prefix by confidence = low σ first) ===")
    print(f"Top {100 * len(top_half) / n:.0f}% (lowest σ): accuracy = {acc_top:.1%}")
    print(f"Rest (higher σ):            accuracy = {acc_bot:.1%}")
    print(f"Lift (low-σ prefix − rest): {acc_top - acc_bot:+.1%}")
    print("")
    if auroc is not None:
        if auroc > 0.6:
            print(f"AUROC={auroc:.3f} > 0.6 → σ ranks with correctness (this run) ✓")
        elif auroc > 0.5:
            print(f"AUROC={auroc:.3f} > 0.5 → weak separation vs random")
        else:
            print(f"AUROC={auroc:.3f} ≤ 0.5 → no separation on this run (report honestly) ✗")

    RISK_CSV.parent.mkdir(parents=True, exist_ok=True)
    with RISK_CSV.open("w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["coverage", "risk", "accuracy"])
        for k in range(len(risks)):
            w.writerow(
                [
                    f"{coverages[k]:.6f}",
                    f"{risks[k]:.6f}",
                    f"{accs[k]:.6f}",
                ]
            )
    print("")
    print(f"Risk-coverage data: {RISK_CSV}")

    rc_for_plot = [
        {"cov": coverages[k], "acc": accs[k]} for k in range(len(coverages))
    ]
    print_ascii_curve(rc_for_plot)
    print_comparison_table(rows)

    md_extra = []
    md_extra.append("## AUROC / AURC / Risk–coverage\n\n")
    md_extra.append("*(Appended by `scripts/real/compute_auroc.py`.)*\n\n")
    if auroc is not None:
        md_extra.append(f"- **AUROC:** {auroc:.3f}\n")
    else:
        md_extra.append(f"- **AUROC:** undefined ({reason})\n")
    md_extra.append(f"- **AURC:** {aurc:.4f}\n")
    md_extra.append(f"- **Correct / incorrect:** {n_cor} / {n_inc}\n")
    md_extra.append(
        f"- **Lowest-σ prefix accuracy** (top {len(top_half)}/{n}): **{acc_top:.1%}**; "
        f"**rest:** **{acc_bot:.1%}**; lift **{acc_top - acc_bot:+.1%}**\n"
    )
    md_extra.append(f"- **risk_coverage.csv:** `{RISK_CSV.relative_to(ROOT)}`\n")

    if not args.no_append_md:
        append_markdown(COMPARISON_MD, "".join(md_extra))

    return 0


if __name__ == "__main__":
    sys.exit(main())
