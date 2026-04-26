#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""Aggregate benchmark artefacts into benchmarks/DEFINITIVE_RESULTS.md (honest, no fabrication)."""

from __future__ import annotations

import argparse
import csv
import datetime as _dt
import os
import sys
from pathlib import Path

_SCRIPT_DIR = Path(__file__).resolve().parent
if str(_SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(_SCRIPT_DIR))

from metacognitive_lib import classify_profile, load_graded_rows, wrong_confident_low_sigma

ROOT = Path(__file__).resolve().parents[2]
OUT_MD = ROOT / "benchmarks" / "DEFINITIVE_RESULTS.md"
GRADED_RESULTS = ROOT / "benchmarks" / "graded" / "graded_results.csv"
CONFORMAL_CSV = ROOT / "benchmarks" / "graded" / "conformal_thresholds.csv"
RESULTS_MD = ROOT / "benchmarks" / "graded" / "RESULTS.md"
CHART_TXT = ROOT / "benchmarks" / "sigma_separation" / "chart.txt"
PIPELINE_CSV = ROOT / "benchmarks" / "sigma_separation" / "pipeline_results.csv"
TRUTH_CSV = ROOT / "benchmarks" / "truthfulqa" / "truthfulqa_results.csv"
EVOLVE_CSV = ROOT / "benchmarks" / "evolve_campaign" / "sigma_trajectory.csv"


def _read_text(p: Path, limit: int = 8000) -> str:
    if not p.is_file():
        return ""
    try:
        return p.read_text(encoding="utf-8")[:limit]
    except OSError:
        return ""


def _conformal_90() -> tuple[str, str]:
    if not CONFORMAL_CSV.is_file():
        return "—", "—"
    with CONFORMAL_CSV.open(encoding="utf-8", newline="") as f:
        for row in csv.DictReader(f):
            try:
                alpha = float(row.get("alpha", "nan"))
            except (TypeError, ValueError):
                continue
            if abs(alpha - 0.10) < 1e-6:
                tau = row.get("tau", "—")
                cov = row.get("test_coverage", "—")
                return str(tau), str(cov)
    return "—", "—"


def _truthfulqa_summary() -> str:
    if not TRUTH_CSV.is_file():
        return "_TruthfulQA not run — missing `benchmarks/truthfulqa/truthfulqa_results.csv`._\n"
    rows: list[dict[str, str]] = []
    with TRUTH_CSV.open(encoding="utf-8", newline="") as f:
        rows = list(csv.DictReader(f))
    if not rows:
        return "_TruthfulQA CSV empty._\n"
    n = len(rows)
    baseline = sum(int(r.get("correct") or 0) for r in rows) / n
    accepted = [r for r in rows if (r.get("action") or "").upper() != "ABSTAIN"]
    acc_acc = (
        sum(int(r.get("correct") or 0) for r in accepted) / len(accepted)
        if accepted
        else 0.0
    )
    truthful = sum(int(r.get("truthful") or 0) for r in rows) / n
    correct_s: list[float] = []
    wrong_s: list[float] = []
    for r in rows:
        if (r.get("action") or "").upper() == "ABSTAIN":
            continue
        try:
            s = float(r.get("sigma") or "")
        except ValueError:
            continue
        if int(r.get("correct") or 0):
            correct_s.append(s)
        else:
            wrong_s.append(s)
    from metacognitive_lib import auroc_correct_wrong

    ar = auroc_correct_wrong(correct_s, wrong_s)
    ar_s = f"{ar:.4f}" if ar is not None else "undefined"
    return (
        f"| Metric | Value |\n|---|---|\n"
        f"| N | {n} |\n"
        f"| Baseline accuracy (letter match, all) | {baseline:.1%} |\n"
        f"| Accuracy (non-ABSTAIN) | {acc_acc:.1%} |\n"
        f"| Truthful rate (correct + ABSTAIN) | {truthful:.1%} |\n"
        f"| AUROC (answered, σ vs letter-correct) | {ar_s} |\n"
    )


def _evolve_summary() -> str:
    if not EVOLVE_CSV.is_file():
        return "_Evolve campaign not run — missing `benchmarks/evolve_campaign/sigma_trajectory.csv`._\n"
    runs: list[tuple[int, float]] = []
    with EVOLVE_CSV.open(encoding="utf-8", newline="") as f:
        for row in csv.DictReader(f):
            try:
                runs.append((int(row["run"]), float(row["sigma_mean"])))
            except (KeyError, ValueError):
                continue
    runs.sort(key=lambda x: x[0])
    if not runs:
        return "_Evolve trajectory parse failed._\n"
    vals = [v for _, v in runs]
    lines = [f"- Run {k}: σ_mean **{v:.4f}**" for k, v in runs[:10]]
    delta = vals[-1] - vals[0] if len(vals) >= 2 else 0.0
    lines.append(f"- Δ (last − first): **{delta:+.4f}**")
    return "\n".join(lines) + "\n"


def _sigma_sep_block() -> str:
    ch = _read_text(CHART_TXT, 4000)
    if ch.strip():
        return "```text\n" + ch.rstrip() + "\n```\n"
    if PIPELINE_CSV.is_file():
        cats: dict[str, list[float]] = {}
        with PIPELINE_CSV.open(encoding="utf-8", newline="") as f:
            for row in csv.DictReader(f):
                try:
                    cats.setdefault(row["category"], []).append(float(row["sigma"]))
                except (KeyError, ValueError):
                    continue
        if not cats:
            return "_No σ-separation CSV rows._\n"
        lines = ["| category | mean σ | n |", "|---|---:|---:|"]
        for c, xs in sorted(cats.items()):
            lines.append(f"| {c} | {sum(xs)/len(xs):.4f} | {len(xs)} |")
        return "\n".join(lines) + "\n"
    return "_σ-separation not run — add `chart.txt` or `pipeline_results.csv`._\n"


def main() -> int:
    ap = argparse.ArgumentParser(description="Write benchmarks/DEFINITIVE_RESULTS.md")
    ap.add_argument(
        "--out",
        type=Path,
        default=OUT_MD,
        help="Output markdown path",
    )
    args = ap.parse_args()
    out: Path = args.out
    pr = None

    utc = _dt.datetime.now(tz=_dt.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    model = os.environ.get("COS_BITNET_CHAT_MODEL", "gemma3:4b")
    hw = os.environ.get(
        "DEFINITIVE_HW",
        "Host not pinned in env; use a non–iCloud clone (see benchmarks/FULL_REPORT.md).",
    )

    md: list[str] = []
    md.append("# Creation OS — Definitive benchmark results\n\n")
    md.append(
        "*This file is generated by `scripts/real/generate_definitive_results.py`. "
        "Numbers come only from on-disk artefacts produced by the suite; "
        "missing sections are explicit._\n\n"
    )
    md.append(f"**Generated (UTC):** {utc}  \n")
    md.append(f"**Model:** `{model}`  \n")
    md.append(f"**Hardware note:** {hw}  \n\n")
    md.append("---\n\n")

    # --- Metacognitive profile ---
    md.append("## Metacognitive profile (graded)\n\n")
    if not GRADED_RESULTS.is_file():
        md.append("_Graded benchmark not run — missing `benchmarks/graded/graded_results.csv`._\n\n")
        pr = None
    else:
        rows = load_graded_rows(GRADED_RESULTS)
        if len(rows) < 10:
            md.append(
                f"_Only **{len(rows)}** numeric-σ graded rows; need ≥10 for profile._\n\n"
            )
            pr = None
        else:
            pr = classify_profile(rows)
            wc = wrong_confident_low_sigma(rows, 0.3)
            md.append(f"| Field | Value |\n|---|---|\n")
            md.append(f"| Profile | **{pr.profile}** |\n")
            md.append(f"| Mean σ | {pr.mean_sigma:.4f} |\n")
            md.append(f"| Abstain rate | {pr.abstain_rate:.1%} |\n")
            ar = "—" if pr.auroc is None else f"{pr.auroc:.4f}"
            md.append(f"| AUROC | {ar} |\n")
            md.append(f"| Accuracy (all) | {pr.acc_all:.1%} |\n")
            md.append(f"| Accuracy (accepted) | {pr.acc_accepted:.1%} |\n")
            md.append(f"| Control gain (accepted − all) | {pr.control_gain:+.1%} |\n")
            md.append(f"| Wrong + confident (σ<0.3, incorrect) | {wc} |\n\n")
            md.append(f"> {pr.description}\n\n")

    md.append("---\n\n")
    md.append("## σ-Separation (15-prompt pipeline)\n\n")
    md.append(_sigma_sep_block())
    md.append("\n---\n\n")

    md.append("## Graded benchmark (50 prompts)\n\n")
    if not GRADED_RESULTS.is_file():
        md.append("_No `graded_results.csv`._\n\n")
    else:
        rows = load_graded_rows(GRADED_RESULTS)
        if not rows:
            md.append("_No numeric-σ rows._\n\n")
        else:
            n = len(rows)
            ca = sum(r["correct"] for r in rows) / n
            acc = [r for r in rows if r["action"] != "ABSTAIN"]
            cacc = sum(r["correct"] for r in acc) / len(acc) if acc else 0.0
            md.append(f"- **N (numeric σ):** {n}\n")
            md.append(f"- **Accuracy (all):** {ca:.1%}\n")
            md.append(f"- **Accuracy (accepted):** {cacc:.1%}\n")
            md.append(f"- **Lift:** {cacc - ca:+.1%}\n")
            md.append(
                f"- **Wrong + confident (σ<0.3):** {wrong_confident_low_sigma(rows)}\n\n"
            )
            rm = _read_text(RESULTS_MD, 12000)
            if rm.strip():
                md.append("See also `benchmarks/graded/RESULTS.md` (AUROC / AURC / ECE / conformal split).\n\n")

    md.append("---\n\n")
    md.append("## Conformal-style split (graded)\n\n")
    tau, cov = _conformal_90()
    md.append(
        f"| Target (1−α) | τ (cal) | Test coverage (held-out half) |\n"
        f"|---|---:|---:|\n"
        f"| 90% (α=0.10) | {tau} | {cov} |\n\n"
    )
    if not CONFORMAL_CSV.is_file():
        md.append("_No `conformal_thresholds.csv` — run graded + `compute_auroc.py` with ≥10 rows._\n\n")

    md.append("---\n\n")
    md.append("## TruthfulQA (MC1)\n\n")
    md.append(_truthfulqa_summary())
    md.append("\n---\n\n")

    md.append("## Evolve campaign (5×10)\n\n")
    md.append(_evolve_summary())

    md.append("\n---\n\n")
    md.append("## One-sentence claim (only if metrics support it)\n\n")
    if pr is not None and pr.auroc is not None and pr.auroc > 0.6:
        md.append(
            f"Creation OS shows **selective sensitivity** on this graded slice "
            f"(AUROC **{pr.auroc:.2f}**): σ tends to track correctness under the σ-gate.\n"
        )
    else:
        md.append(
            "_Do not use a selective-sensitivity headline until AUROC on graded data "
            "is clearly above chance with sufficient incorrect answered rows._\n"
        )

    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text("".join(md), encoding="utf-8")
    print(f"Wrote {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
