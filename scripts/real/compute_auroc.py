#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""AUROC, AURC, risk–coverage, split conformal-style τ calibration, ECE, RESULTS.md.

Run after `graded_benchmark.py` / `graded_benchmark.sh`.
Reads:  benchmarks/graded/graded_results.csv
Writes: benchmarks/graded/risk_coverage.csv
         benchmarks/graded/conformal_thresholds.csv (if ≥10 rows)
         benchmarks/graded/RESULTS.md
Appends: benchmarks/graded/graded_comparison.md (if present; short AUROC summary)
"""
from __future__ import annotations

import argparse
import csv
import datetime as _dt
import math
import random
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_CSV = ROOT / "benchmarks" / "graded" / "graded_results.csv"
RISK_CSV = ROOT / "benchmarks" / "graded" / "risk_coverage.csv"
CONFORMAL_CSV = ROOT / "benchmarks" / "graded" / "conformal_thresholds.csv"
RESULTS_MD = ROOT / "benchmarks" / "graded" / "RESULTS.md"
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


def conformal_find_tau(cal: list[dict], target_acc: float) -> float:
    """Smallest τ in sorted unique calibration σ such that acc({σ≤τ}) ≥ target_acc."""
    if not cal:
        return 1.0
    sigs = sorted(set(r["sigma"] for r in cal))
    best_tau = max(sigs)
    for tau in sigs:
        acc = [r for r in cal if r["sigma"] <= tau]
        if not acc:
            continue
        a = sum(r["correct"] for r in acc) / len(acc)
        if a >= target_acc:
            return tau
    return best_tau


def conformal_evaluate(
    cal: list[dict], test: list[dict], alphas: tuple[float, ...]
) -> tuple[list[dict], list[str]]:
    """Return CSV rows + human-readable lines."""
    lines_out: list[str] = []
    csv_rows: list[dict] = []
    lines_out.append("=== CONFORMAL-STYLE SPLIT CALIBRATION (τ from cal, check test) ===")
    lines_out.append(f"Calibration set: {len(cal)} prompts | Test set: {len(test)} prompts")
    lines_out.append("")
    lines_out.append(
        "Disclaimer: this is an **empirical split** (shuffle seed 42, first half / "
        "second half). It is **not** a full distribution-free conformal guarantee "
        "without the usual CP assumptions and finite-sample correction; test accuracy "
        "can still fall below the target. Report ✓/✗ honestly."
    )
    lines_out.append("")

    for alpha in alphas:
        target_acc = 1.0 - alpha
        tau = conformal_find_tau(cal, target_acc)
        ta = [r for r in test if r["sigma"] <= tau]
        test_cov = len(ta) / len(test) if test else 0.0
        test_acc = sum(r["correct"] for r in ta) / len(ta) if ta else 0.0
        ok = test_acc >= target_acc
        mark = "OK" if ok else "FAIL"
        lines_out.append(f"Target mean accuracy on accepted (cal): ≥ {target_acc:.0%} (α={alpha})")
        lines_out.append(f"  τ = {tau:.4f}")
        lines_out.append(
            f"  Test: accuracy={test_acc:.1%} {mark} | coverage={test_cov:.0%} "
            f"({len(ta)}/{len(test)} accepted)"
        )
        lines_out.append("")
        csv_rows.append(
            {
                "alpha": alpha,
                "target_accuracy": target_acc,
                "tau": tau,
                "test_accuracy": test_acc,
                "test_coverage": test_cov,
                "test_ok": int(ok),
            }
        )
    return csv_rows, lines_out


def ece_bins(rows: list[dict], n_bins: int = 5) -> tuple[float, list[str]]:
    """ECE with confidence = 1 − σ; bins uniform in [0,1)."""
    lines: list[str] = []
    lines.append("=== CALIBRATION (ECE) ===")
    lines.append(f"Confidence proxy: 1 − σ_combined; bins = {n_bins}")
    lines.append(f"{'Bin':>6} {'Conf':>8} {'Acc':>8} {'Gap':>8} {'N':>5}")
    bins: list[list[dict]] = [[] for _ in range(n_bins)]
    for r in rows:
        conf = max(0.0, min(1.0, 1.0 - r["sigma"]))
        b = min(int(conf * n_bins), n_bins - 1)
        bins[b].append(r)
    ece = 0.0
    total = len(rows)
    if total == 0:
        return 0.0, lines
    for i, b in enumerate(bins):
        if not b:
            continue
        conf = sum(max(0.0, min(1.0, 1.0 - r["sigma"])) for r in b) / len(b)
        acc = sum(r["correct"] for r in b) / len(b)
        gap = abs(acc - conf)
        ece += gap * (len(b) / total)
        lines.append(f"{i + 1:>6} {conf:>8.3f} {acc:>8.3f} {gap:>8.3f} {len(b):>5}")
    lines.append("")
    lines.append(f"ECE = {ece:.4f}")
    lines.append("  (0.0 = perfect calibration on this proxy)")
    lines.append("  (<0.10 = often called well calibrated heuristically)")
    lines.append("  (>0.20 = poorly calibrated on this proxy — report honestly)")
    return ece, lines


def tau_accuracy_on_full(
    rows: list[dict], target_acc: float, min_n: int = 3
) -> tuple[float | None, float | None, float | None]:
    """Smallest τ among sorted unique σ (full set) with ≥min_n points and prefix acc ≥ target."""
    rows_s = sorted(rows, key=lambda r: r["sigma"])
    sigs = sorted(set(r["sigma"] for r in rows_s))
    for tau in sigs:
        sub = [r for r in rows_s if r["sigma"] <= tau]
        if len(sub) < min_n:
            continue
        acc = sum(r["correct"] for r in sub) / len(sub)
        if acc >= target_acc:
            cov = len(sub) / len(rows)
            return tau, cov, acc
    return None, None, None


def print_definitive_table(rows: list[dict], model_note: str) -> list[str]:
    n = len(rows)
    lines: list[str] = []
    if not n:
        return lines
    correct_all = sum(r["correct"] for r in rows)
    acc_all = correct_all / n
    accepted = [r for r in rows if r["action"] != "ABSTAIN"]
    acc_acc = sum(r["correct"] for r in accepted) / len(accepted) if accepted else 0.0
    abstained = n - len(accepted)
    tau_90, cov_90, acc_pref = tau_accuracy_on_full(rows, 0.9, min_n=3)
    wrong_confident = sum(
        1 for r in rows if r["correct"] == 0 and r["sigma"] < 0.3
    )

    lines.append("=" * 55)
    lines.append("CREATION OS σ-GATE: DEFINITIVE RESULTS (this CSV)")
    lines.append(model_note)
    lines.append(f"Prompts: {n}")
    lines.append("=" * 55)
    lines.append(f"{'Metric':<35} {'Value':>15}")
    lines.append("-" * 55)
    lines.append(f"{'Accuracy (all answers)':<35} {acc_all:>14.1%}")
    lines.append(f"{'Accuracy (σ-accepted only, non-ABSTAIN)':<35} {acc_acc:>14.1%}")
    lines.append(f"{'Accuracy lift (answered − all)':<35} {acc_acc - acc_all:>+14.1%}")
    lines.append(f"{'Abstention rate':<35} {abstained / n:>14.1%}")
    lines.append(f"{'Wrong + low σ (σ<0.3, incorrect)':<35} {wrong_confident:>15}")
    if tau_90 is not None:
        lines.append(f"{'Empirical 90% acc. threshold (τ, full set)':<35} {tau_90:>15.4f}")
        lines.append(f"{'Coverage at that τ (fraction of prompts)':<35} {cov_90:>14.1%}")
        lines.append(f"{'Accuracy at that τ (prefix)':<35} {acc_pref:>14.1%}")
    lines.append("=" * 55)
    for s in lines:
        print(s)
    print("")
    if acc_acc > acc_all + 0.05:
        print("Answered-only accuracy > all-rows + 5pp (abstention / grader interaction).")
    if wrong_confident == 0:
        print("ZERO incorrect rows with σ<0.3 (full table definition).")
    if tau_90 is not None:
        print(
            f"Empirical τ for ≥90% accuracy on lowest-σ mass (min n=3): τ={tau_90:.4f} "
            f"(not a finite-sample conformal certificate by itself)."
        )
    return lines


def append_markdown(path: Path, text: str) -> None:
    if not path.is_file():
        return
    with path.open("a", encoding="utf-8") as f:
        f.write("\n\n")
        f.write(text)


def main() -> int:
    ap = argparse.ArgumentParser(
        description="AUROC / AURC / conformal-style split / ECE / RESULTS.md"
    )
    ap.add_argument("--csv", type=Path, default=DEFAULT_CSV, help="graded_results.csv")
    ap.add_argument("--no-append-md", action="store_true")
    ap.add_argument(
        "--model",
        default="gemma3:4b (set COS_BITNET_CHAT_MODEL when running graded bench)",
        help="Note printed in RESULTS.md",
    )
    args = ap.parse_args()
    path: Path = args.csv
    if not path.is_file():
        print(f"error: missing {path}", file=sys.stderr)
        return 1

    rows = load_rows(path)
    if len(rows) < 1:
        print("No numeric-σ rows.", file=sys.stderr)
        return 1

    rows_sorted = sorted(rows, key=lambda r: r["sigma"])
    n = len(rows_sorted)
    out_lines: list[str] = []

    # --- AUROC / AURC (need ≥4) ---
    auroc = None
    n_cor = n_inc = 0
    reason = ""
    aurc = 0.0
    risks: list[float] = []
    coverages: list[float] = []
    accs: list[float] = []
    if n >= 4:
        auroc, n_cor, n_inc, reason = auroc_value(rows_sorted)
        risks, coverages, accs = risk_coverage_series(rows_sorted)
        aurc = trapezoid_aurc(coverages, risks)
        print("=== AUROC / AURC RESULTS ===")
        if auroc is None:
            print(f"AUROC: undefined ({reason}; correct={n_cor}, incorrect={n_inc})")
        else:
            print(f"AUROC: {auroc:.3f}")
            print(
                "  (1.0 = perfect rank; 0.5 = random; <0.5 = inverted vs this convention)"
            )
        print(f"AURC:  {aurc:.4f}")
        print("  (integrated risk vs coverage; lower = better under this curve)")
        print("")
        print(f"Correct: {n_cor} | Incorrect: {n_inc}")
        print("")
        print_risk_table(rows_sorted, n)
        h = n // 2
        top_half = rows_sorted[:h] if h else []
        bot_half = rows_sorted[h:] if h < n else []
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
    else:
        print(f"AUROC/AURC skipped (n={n} < 4).")

    print_comparison_table(rows_sorted)

    # --- Conformal-style split (≥10) ---
    conformal_lines: list[str] = []
    if n >= 10:
        rng = random.Random(42)
        shuf = list(rows_sorted)
        rng.shuffle(shuf)
        n_cal = n // 2
        cal, test = shuf[:n_cal], shuf[n_cal:]
        alphas = (0.05, 0.10, 0.15, 0.20, 0.30)
        csv_conf, conformal_lines = conformal_evaluate(cal, test, alphas)
        for ln in conformal_lines:
            print(ln)
        CONFORMAL_CSV.parent.mkdir(parents=True, exist_ok=True)
        with CONFORMAL_CSV.open("w", newline="", encoding="utf-8") as f:
            w = csv.DictWriter(
                f,
                fieldnames=[
                    "alpha",
                    "target_accuracy",
                    "tau",
                    "test_accuracy",
                    "test_coverage",
                    "test_ok",
                ],
            )
            w.writeheader()
            w.writerows(csv_conf)
        print(f"Thresholds: {CONFORMAL_CSV}")
    else:
        conformal_lines = [
            f"Conformal split skipped (n={n} < 10).",
        ]
        print(conformal_lines[0])

    # --- ECE ---
    ece_val, ece_lines = ece_bins(rows_sorted, 5)
    for ln in ece_lines:
        print(ln)

    # --- Definitive block ---
    def_lines = print_definitive_table(rows_sorted, f"Model note: {args.model}")

    # --- RESULTS.md ---
    ts = _dt.datetime.now(tz=_dt.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    try:
        path_rel = path.relative_to(ROOT)
    except ValueError:
        path_rel = path
    md: list[str] = []
    md.append("# Graded benchmark — full metrics\n\n")
    md.append(f"**Generated (UTC):** {ts}  \n")
    md.append(f"**Source CSV:** `{path_rel}`  \n\n")
    md.append(
        "## Scope and honesty\n\n"
        "- **AUROC / AURC / ECE** are standard *reporting* quantities on this run.\n"
        "- **Split τ search** uses half the shuffled rows for threshold selection and "
        "half for reporting test accuracy; finite-sample **test** accuracy can fail "
        "the target — the table marks **OK/FAIL**.\n"
        "- This is **not** a substitute for a full conformal prediction proof; "
        "exchangeability with future prompts is an **assumption**.\n\n"
    )

    if n >= 4 and auroc is not None:
        md.append("## AUROC / AURC\n\n")
        md.append(f"- **AUROC:** {auroc:.4f}\n")
        md.append(f"- **AURC:** {aurc:.4f}\n")
        md.append(f"- **Correct / incorrect:** {n_cor} / {n_inc}\n\n")
    elif n >= 4:
        md.append("## AUROC / AURC\n\n")
        md.append(f"- **AUROC:** undefined ({reason})\n")
        md.append(f"- **AURC:** {aurc:.4f}\n\n")
    else:
        md.append("## AUROC / AURC\n\nSkipped (insufficient rows).\n\n")

    md.append("## Conformal-style split calibration\n\n")
    md.append("```text\n")
    md.extend([ln + "\n" for ln in conformal_lines])
    md.append("```\n\n")

    md.append("## ECE (1 − σ proxy)\n\n")
    md.append("```text\n")
    md.extend([ln + "\n" for ln in ece_lines])
    md.append("```\n\n")

    md.append("## Definitive table\n\n")
    md.append("```text\n")
    md.extend([ln + "\n" for ln in def_lines])
    md.append("```\n\n")

    md.append(
        "## Files\n\n"
        f"- `{RISK_CSV.relative_to(ROOT)}`\n"
    )
    if n >= 10:
        md.append(f"- `{CONFORMAL_CSV.relative_to(ROOT)}`\n")
    md.append("\n")

    RESULTS_MD.parent.mkdir(parents=True, exist_ok=True)
    RESULTS_MD.write_text("".join(md), encoding="utf-8")
    print("")
    print(f"Wrote {RESULTS_MD}")

    if not args.no_append_md and n >= 4:
        md_extra = ["## AUROC / AURC / Risk–coverage\n\n"]
        md_extra.append("*(Appended by `scripts/real/compute_auroc.py`.)*\n\n")
        if auroc is not None:
            md_extra.append(f"- **AUROC:** {auroc:.3f}\n")
        else:
            md_extra.append(f"- **AUROC:** undefined ({reason})\n")
        md_extra.append(f"- **AURC:** {aurc:.4f}\n")
        md_extra.append(
            f"- **Full report:** `{RESULTS_MD.relative_to(ROOT)}` "
            "(conformal split + ECE + definitive table)\n"
        )
        append_markdown(COMPARISON_MD, "".join(md_extra))

    return 0


if __name__ == "__main__":
    sys.exit(main())
