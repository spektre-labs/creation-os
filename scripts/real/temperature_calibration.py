#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""Temperature scaling on confidence = 1−σ (graded rows); ECE before/after; optional Platt.

Writes:
  benchmarks/graded/temperature.txt  — optimal T (one line)
  benchmarks/graded/platt.txt        — Platt w b (two floats, logistic on raw conf)

No SciPy required (bounded grid search). Install SciPy for `minimize_scalar` if desired.
"""
from __future__ import annotations

import argparse
import csv
import math
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_CSV = ROOT / "benchmarks" / "graded" / "graded_results.csv"
OUT_T = ROOT / "benchmarks" / "graded" / "temperature.txt"
OUT_PLATT = ROOT / "benchmarks" / "graded" / "platt.txt"


def logit(p: float) -> float:
    p = max(min(p, 0.999), 0.001)
    return math.log(p / (1.0 - p))


def sigmoid(x: float) -> float:
    if x >= 35.0:
        return 1.0
    if x <= -35.0:
        return 0.0
    return 1.0 / (1.0 + math.exp(-x))


def load_rows(path: Path) -> list[dict]:
    rows: list[dict] = []
    with path.open(newline="", encoding="utf-8") as f:
        for r in csv.DictReader(f):
            try:
                rows.append(
                    {"sigma": float(r["sigma"]), "correct": int(r["is_correct"])}
                )
            except (TypeError, ValueError, KeyError):
                continue
    return rows


def nll_temperature(cal: list[dict], t: float) -> float:
    if t <= 1e-6:
        return 1e9
    loss = 0.0
    for r in cal:
        conf = max(min(1.0 - r["sigma"], 0.999), 0.001)
        scaled = sigmoid(logit(conf) / t)
        if r["correct"]:
            loss -= math.log(max(scaled, 1e-10))
        else:
            loss -= math.log(max(1.0 - scaled, 1e-10))
    return loss / max(len(cal), 1)


def optimal_temperature_grid(cal: list[dict], lo: float = 0.1, hi: float = 10.0, steps: int = 500) -> float:
    best_t = lo
    best_v = nll_temperature(cal, lo)
    for i in range(steps + 1):
        t = lo + (hi - lo) * (i / steps)
        v = nll_temperature(cal, t)
        if v < best_v:
            best_v, best_t = v, t
    return best_t


def try_scipy_minimize(cal: list[dict]) -> float | None:
    try:
        from scipy.optimize import minimize_scalar  # type: ignore
    except ImportError:
        return None

    res = minimize_scalar(
        lambda t: nll_temperature(cal, t),
        bounds=(0.1, 10.0),
        method="bounded",
    )
    if res.success or math.isfinite(res.fun):
        return float(res.x)
    return None


def ece_from_conf(rows: list[dict], conf_fn, n_bins: int = 5) -> float:
    if not rows:
        return 0.0
    bins: list[list[tuple[float, int]]] = [[] for _ in range(n_bins)]
    for r in rows:
        conf = max(0.0, min(1.0, conf_fn(r)))
        b = min(int(conf * n_bins), n_bins - 1)
        bins[b].append((conf, r["correct"]))
    total = len(rows)
    ece = 0.0
    for b in bins:
        if not b:
            continue
        conf_m = sum(x[0] for x in b) / len(b)
        acc_m = sum(x[1] for x in b) / len(b)
        ece += abs(acc_m - conf_m) * (len(b) / total)
    return ece


def platt_grid_fit(cal: list[dict]) -> tuple[float, float, float]:
    """Minimize NLL over w,b for P(y=1)=sigmoid(w*conf_raw+b), conf_raw=1-sigma."""
    best = (0.0, 0.0, 1e9)
    for w in [i * 1.5 for i in range(-20, 21)]:
        for b in [j * 0.5 for j in range(-20, 21)]:
            nll = 0.0
            for r in cal:
                conf = max(min(1.0 - r["sigma"], 0.999), 0.001)
                p = sigmoid(w * conf + b)
                if r["correct"]:
                    nll -= math.log(max(p, 1e-10))
                else:
                    nll -= math.log(max(1.0 - p, 1e-10))
            nll /= max(len(cal), 1)
            if nll < best[2]:
                best = (w, b, nll)
    return best[0], best[1], best[2]


def main() -> int:
    ap = argparse.ArgumentParser(description="Temperature scaling + Platt on graded σ")
    ap.add_argument("--csv", type=Path, default=DEFAULT_CSV)
    args = ap.parse_args()
    path: Path = args.csv
    if not path.is_file():
        print(f"error: missing {path}", file=sys.stderr)
        return 1

    rows = load_rows(path)
    n = len(rows)
    if n < 4:
        print(f"Too few rows ({n}); need ≥4 for calibration split.")
        return 1

    n_cal = max(2, int(n * 0.6))
    cal, test = rows[:n_cal], rows[n_cal:]
    if len(test) < 1:
        print("Empty test split.", file=sys.stderr)
        return 1

    t_opt = try_scipy_minimize(cal)
    if t_opt is None:
        t_opt = optimal_temperature_grid(cal)

    print("=== TEMPERATURE SCALING (confidence = 1 − σ) ===")
    print(f"Optimal T: {t_opt:.4f}")
    print("  T < 1: underconfident on this proxy → scale UP confidence spread")
    print("  T > 1: overconfident → scale DOWN")
    print("  T ≈ 1: close to identity in logit space")
    print("")
    print("=== BEFORE vs AFTER (test set, same 5-bin ECE) ===")

    def conf_before(r: dict) -> float:
        return max(0.0, min(1.0, 1.0 - r["sigma"]))

    def conf_after(r: dict) -> float:
        c = max(min(1.0 - r["sigma"], 0.999), 0.001)
        return sigmoid(logit(c) / t_opt)

    ece_b = ece_from_conf(test, conf_before, 5)
    ece_a = ece_from_conf(test, conf_after, 5)
    print(f"ECE before: {ece_b:.4f}")
    print(f"ECE after:  {ece_a:.4f}")
    print(f"Change:     {ece_a - ece_b:+.4f}  (negative = improvement)")
    print("")

    w, b, platt_nll = platt_grid_fit(cal)
    print("=== PLATT (logistic on raw conf = 1−σ, fit on cal) ===")
    print(f"w={w:.4f} b={b:.4f}  (cal NLL={platt_nll:.4f})")
    print("  P(correct|σ) ≈ sigmoid(w·(1−σ) + b)")

    def conf_platt(r: dict) -> float:
        c = max(min(1.0 - r["sigma"], 0.999), 0.001)
        return max(0.0, min(1.0, sigmoid(w * c + b)))

    ece_p = ece_from_conf(test, conf_platt, 5)
    print(f"ECE (test, Platt conf): {ece_p:.4f}")
    print("")

    OUT_T.parent.mkdir(parents=True, exist_ok=True)
    OUT_T.write_text(f"{t_opt:.8f}\n", encoding="utf-8")
    OUT_PLATT.write_text(f"{w:.8f} {b:.8f}\n", encoding="utf-8")
    print(f"Wrote {OUT_T}")
    print(f"Wrote {OUT_PLATT}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
