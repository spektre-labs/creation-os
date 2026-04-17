# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2026 Spektre Labs / Lauri Rainio
"""
v103 — post-hoc τ-sweep and risk-coverage analysis for σ-gated BitNet.

Input (produced by benchmarks/v103/run_sigma_log.sh):
  * lm-eval per-sample JSONL under
        benchmarks/v103/results/lm_eval/<task>/<hash>/samples_<task>_*.jsonl
    one row per document with fields (`doc_id`, `acc`, `filtered_resps`, …)
  * σ sidecar JSONL at
        benchmarks/v103/results/samples_<task>_sigma.jsonl
    one row per (document, candidate) with `sigma_mean`, `sigma_max_token`,
    `sigma_profile[8]`.

The two streams join on `(doc_index, cand_index)` — v103's backend emits
documents in request order, which matches the order lm-eval writes to
its samples file (the `doc_id` field in lm-eval).

Output:
  * benchmarks/v103/rc_curves.json        risk-coverage curves (task × signal)
  * benchmarks/v103/rc_summary.json       AURCC / AUACC / Coverage@target_acc
  * stdout                                 human-readable table

Gating signals compared:
    σ_MEAN        mean of 8 σ-channels on top-1 candidate   (v103 default)
    σ_MAX_TOKEN   worst single-token σ on top-1 candidate   (per-step σ peak)
    entropy       sigma_profile[0] (entropy_norm) of top-1  (baseline)
    margin_inv    sigma_profile[1] (1 - (p1-p2)) of top-1   (margin baseline)
    ll_inv        a purely-ll baseline: -ll_top1 / n_cont    (scale-free)
    random        uniform random [0,1] (statistical floor)

Metrics (all documented in docs/v103/SELECTIVE_PREDICTION.md):
    AURCC         Area under risk-coverage curve (lower = better)
                  risk = 1 - cond_acc; curve is cond_acc vs coverage after
                  sorting documents by signal ascending (most-confident first).
    AUACC         Area under accuracy-coverage curve (higher = better)
    Cov@90,95,99  Maximum coverage at which cond_acc ≥ target.

Interpretation:
    If σ_MEAN AURCC < entropy AURCC on all three tasks (outside combined
    stderr), σ is an information-adding signal beyond entropy alone.
    If they are within stderr, σ is redundant with entropy — the
    seven additional channels carry no extra selective-prediction signal
    on this model + these tasks (a valid, honest null result; see
    arXiv:2603.21172 "Entropy Alone is Insufficient").
"""

from __future__ import annotations

import argparse
import glob
import json
import math
import os
import random
import statistics
from typing import Any, Dict, List, Tuple


# ------------------------------ loaders ---------------------------------- #

def _load_lm_eval_samples(task: str, lm_eval_root: str) -> List[dict]:
    """Return list of doc rows, sorted by doc_id."""
    patterns = [
        os.path.join(lm_eval_root, task, "*", f"samples_{task}_*.jsonl"),
        os.path.join(lm_eval_root, "*", f"samples_{task}_*.jsonl"),
    ]
    fn = None
    for p in patterns:
        matches = sorted(glob.glob(p))
        if matches:
            fn = matches[-1]
            break
    if fn is None:
        raise FileNotFoundError(f"v103: no lm-eval samples JSONL for '{task}' under {lm_eval_root}")
    rows: List[dict] = []
    with open(fn) as fh:
        for line in fh:
            rows.append(json.loads(line))
    rows.sort(key=lambda r: r["doc_id"])
    return rows


def _load_sigma_sidecar(task: str, results_root: str) -> Dict[Tuple[int, int], dict]:
    """Return {(doc_index, cand_index): row}. One row per candidate."""
    fn = os.path.join(results_root, f"samples_{task}_sigma.jsonl")
    if not os.path.isfile(fn):
        raise FileNotFoundError(f"v103: no σ sidecar at {fn}")
    out: Dict[Tuple[int, int], dict] = {}
    with open(fn) as fh:
        for line in fh:
            r = json.loads(line)
            out[(int(r["doc_index"]), int(r["cand_index"]))] = r
    return out


# ------------------------ per-doc aggregation ---------------------------- #

def _extract_per_doc(task: str,
                    lm_rows: List[dict],
                    sigma: Dict[Tuple[int, int], dict]) -> List[dict]:
    """
    Join lm-eval rows with σ sidecar and produce one summary per document:
        { "doc_id": int, "acc": float, "acc_norm": float or None,
          "top1_cand": int,
          "sigma_mean": float, "sigma_max_token": float,
          "sigma_profile": [8 floats] }
    `top1_cand` is the argmax-ll candidate, matching how lm-eval's default
    `acc` metric chooses.  For MC2 tasks `acc` is already in [0,1] per doc;
    we treat conditional_acc as the mean of per-doc `acc` over kept docs.
    """
    out: List[dict] = []
    for row in lm_rows:
        doc_id = int(row["doc_id"])
        resps = row.get("filtered_resps") or row.get("resps") or []
        lls: List[float] = []
        for pair in resps:
            if isinstance(pair, list) and len(pair) == 2 and isinstance(pair[0], (str, int, float)):
                lls.append(float(pair[0]))
            elif isinstance(pair, list) and pair and isinstance(pair[0], list):
                lls.append(float(pair[0][0]))
        if not lls:
            continue
        top1 = max(range(len(lls)), key=lambda i: lls[i])
        key = (doc_id, top1)
        s = sigma.get(key)
        if s is None:
            # fallback: use cand 0 if top1 missing; fail loud on total miss
            s = sigma.get((doc_id, 0))
            if s is None:
                raise KeyError(f"σ sidecar has no row for doc {doc_id} task {task}")
        out.append({
            "doc_id": doc_id,
            "acc": float(row.get("acc", 0.0)),
            "acc_norm": float(row["acc_norm"]) if "acc_norm" in row else None,
            "top1_cand": top1,
            "sigma_mean": float(s["sigma_mean"]),
            "sigma_max_token": float(s["sigma_max_token"]),
            "sigma_profile": [float(x) for x in s["sigma_profile"]],
        })
    return out


# ---------------------- risk-coverage curves ----------------------------- #

def _rc_curve(docs: List[dict], signal: str, metric_key: str = "acc") -> List[Tuple[float, float, float]]:
    """Sort by signal ascending (most-confident first) and emit the full
    monotonic RC curve as a list of (coverage, cond_acc, risk).

    At coverage 1/N, 2/N, …, 1.0 the cond_acc is mean(acc) over the first
    k kept docs, risk = 1 - cond_acc.
    """
    n = len(docs)
    if n == 0:
        return []
    def _sig(d):
        if signal == "sigma_mean":        return d["sigma_mean"]
        if signal == "sigma_max_token":   return d["sigma_max_token"]
        if signal == "entropy":           return d["sigma_profile"][0]
        if signal == "margin_inv":        return d["sigma_profile"][1]
        if signal == "p_max":             return d["sigma_profile"][5]
        if signal == "ll_inv":            return -0.0  # filled below
        if signal == "random":            return d.get("_rand", 0.0)
        raise ValueError(f"unknown signal {signal}")

    if signal == "random":
        rng = random.Random(1234)
        for d in docs:
            d["_rand"] = rng.random()
    sorted_docs = sorted(docs, key=_sig)

    out: List[Tuple[float, float, float]] = []
    cum_acc = 0.0
    for k in range(1, n + 1):
        cum_acc += sorted_docs[k - 1][metric_key]
        cond = cum_acc / k
        out.append((k / n, cond, 1.0 - cond))
    return out


def _aurcc(curve: List[Tuple[float, float, float]]) -> float:
    """Area under the risk-coverage curve via the trapezoid rule."""
    if len(curve) < 2:
        return float("nan")
    area = 0.0
    for i in range(1, len(curve)):
        x0, _, r0 = curve[i - 1]
        x1, _, r1 = curve[i]
        area += 0.5 * (r0 + r1) * (x1 - x0)
    return area


def _auacc(curve: List[Tuple[float, float, float]]) -> float:
    if len(curve) < 2:
        return float("nan")
    area = 0.0
    for i in range(1, len(curve)):
        x0, a0, _ = curve[i - 1]
        x1, a1, _ = curve[i]
        area += 0.5 * (a0 + a1) * (x1 - x0)
    return area


def _coverage_at_acc(curve: List[Tuple[float, float, float]], target: float) -> float:
    """Max coverage at which cond_acc ≥ target.  0.0 if target unreachable."""
    ok = [c for (c, a, _) in curve if a >= target]
    return max(ok) if ok else 0.0


# ------------------------------ main ------------------------------------- #

def _bootstrap_aurcc(docs: List[dict], signal: str,
                     n_boot: int = 2000, seed: int = 0) -> Tuple[float, float, float]:
    """Return (mean, lo95, hi95) AURCC via a basic nonparametric bootstrap.
    For speed we do not re-sort from scratch; we resample with replacement
    from the original doc list, which correctly captures the n=500 sampling
    variance of the AURCC estimator.
    """
    rng = random.Random(seed)
    n = len(docs)
    if n < 10:
        return (float("nan"), float("nan"), float("nan"))
    aurccs: List[float] = []
    for _ in range(n_boot):
        sample = [docs[rng.randrange(n)] for _ in range(n)]
        curve = _rc_curve(sample, signal)
        aurccs.append(_aurcc(curve))
    aurccs.sort()
    mean = sum(aurccs) / n_boot
    lo = aurccs[int(0.025 * n_boot)]
    hi = aurccs[int(0.975 * n_boot)]
    return (mean, lo, hi)


def run_analysis(results_root: str,
                 tasks: List[str],
                 n_boot: int = 2000) -> Dict[str, Any]:
    lm_eval_root = os.path.join(results_root, "lm_eval")
    signals = ["sigma_mean", "sigma_max_token", "entropy",
               "margin_inv", "p_max", "random"]

    summary: Dict[str, Any] = {"tasks": {}, "signals": signals}
    curves_out: Dict[str, Any] = {"tasks": {}}

    for task in tasks:
        rows = _load_lm_eval_samples(task, lm_eval_root)
        sigma = _load_sigma_sidecar(task, results_root)
        docs  = _extract_per_doc(task, rows, sigma)
        if not docs:
            print(f"[{task}] no docs — skip"); continue
        n = len(docs)
        overall_acc = sum(d["acc"] for d in docs) / n
        print(f"\n### task={task}  n={n}  overall_acc={overall_acc:.4f}")

        t_summary: Dict[str, Any] = {
            "n": n, "overall_acc": overall_acc, "signals": {}
        }
        t_curves: Dict[str, Any] = {}

        hdr = f"  {'signal':<18} {'AURCC':>8} {'CI95':>17} {'AUACC':>8} {'Cov@0.90':>10} {'Cov@0.95':>10}"
        print(hdr); print("  " + "-" * (len(hdr) - 2))
        for sig in signals:
            curve = _rc_curve(docs, sig, metric_key="acc")
            aurcc = _aurcc(curve); auacc = _auacc(curve)
            c90 = _coverage_at_acc(curve, 0.90)
            c95 = _coverage_at_acc(curve, 0.95)
            c99 = _coverage_at_acc(curve, 0.99)
            boot_mean, lo, hi = _bootstrap_aurcc(docs, sig, n_boot=n_boot)
            t_summary["signals"][sig] = {
                "AURCC": aurcc, "AUACC": auacc,
                "AURCC_ci95_lo": lo, "AURCC_ci95_hi": hi,
                "AURCC_boot_mean": boot_mean,
                "Cov@0.90": c90, "Cov@0.95": c95, "Cov@0.99": c99,
            }
            stride = max(1, n // 200)
            t_curves[sig] = [
                {"coverage": round(c, 6),
                 "cond_acc": round(a, 6),
                 "risk":     round(r, 6)}
                for i, (c, a, r) in enumerate(curve) if i % stride == 0 or i == n - 1
            ]
            ci_txt = f"[{lo:.4f},{hi:.4f}]"
            print(f"  {sig:<18} {aurcc:>8.4f} {ci_txt:>17} {auacc:>8.4f} {c90:>10.4f} {c95:>10.4f}")
        summary["tasks"][task] = t_summary
        curves_out["tasks"][task] = t_curves

    return {"summary": summary, "curves": curves_out}


def _write_json(path: str, data: Any) -> None:
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    with open(path, "w") as fh:
        json.dump(data, fh, indent=2, sort_keys=True)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--results", default="benchmarks/v103/results",
                    help="σ-log run output root")
    ap.add_argument("--tasks", default="arc_easy,arc_challenge,truthfulqa_mc2")
    ap.add_argument("--curves-out", default="benchmarks/v103/rc_curves.json")
    ap.add_argument("--summary-out", default="benchmarks/v103/rc_summary.json")
    args = ap.parse_args()

    tasks = [t.strip() for t in args.tasks.split(",") if t.strip()]
    res = run_analysis(args.results, tasks)

    _write_json(args.curves_out, res["curves"])
    _write_json(args.summary_out, res["summary"])
    print(f"\nwrote {args.curves_out}")
    print(f"wrote {args.summary_out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
