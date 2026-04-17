# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2026 Spektre Labs / Lauri Rainio
"""
v104 — operator search: compute AURCC + paired bootstrap vs. entropy for
every candidate operator from `operators.py`.

Crucial design choice: the v103 RC analysis used *independent* bootstrap
CIs for each signal, which sum noise when comparing two signals that see
the same documents.  v104 uses **paired bootstrap on the difference
ΔAURCC(op − entropy)** — i.e. for each bootstrap resample we recompute
both curves from the SAME resampled doc list, then take the difference.
This removes the shared doc-level variance and tightens the CI on the
delta by roughly sqrt(2).

Pre-registered pipeline (all ten σ operators are tested against the
entropy baseline, alpha=0.005 after Bonferroni over OPERATORS_BONFERRONI_N
candidates).

Outputs:
    benchmarks/v104/results/operator_comparison.json
    benchmarks/v104/results/operator_comparison.md   (pretty table)
Prints the table to stdout.
"""

from __future__ import annotations

import argparse
import json
import math
import os
import random
import statistics
import sys
from typing import Any, Dict, List, Tuple

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "v103"))

from operators import OPERATORS, BONFERRONI_N  # noqa: E402
from compute_rc_metrics import (  # noqa: E402
    _load_lm_eval_samples,
    _load_sigma_sidecar,
    _aurcc,
    _auacc,
)


# ---------------- joining lm-eval rows + sidecar ---------------- #

def _per_doc_rows(task: str, results_root: str) -> List[dict]:
    """One entry per document with the full sidecar row attached for the
    argmax-ll candidate.  Returns [{'acc', 'sidecar': dict}, ...]."""
    lm_eval_root = os.path.join(results_root, "lm_eval")
    lm_rows = _load_lm_eval_samples(task, lm_eval_root)
    sigma = _load_sigma_sidecar(task, results_root)
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
        s = sigma.get((doc_id, top1)) or sigma.get((doc_id, 0))
        if s is None:
            raise KeyError(f"v104: σ sidecar missing doc {doc_id} in {task}")
        # attach provenance for the random operator
        s = dict(s)
        s["doc_index"] = doc_id
        s["cand_index"] = top1
        out.append({"acc": float(row.get("acc", 0.0)),
                    "sidecar": s})
    return out


# ---------------- RC math on arbitrary operator ---------------- #

def _rc_curve_op(docs: List[dict], op_fn) -> List[Tuple[float, float]]:
    """Return (coverage, cond_acc) points for operator `op_fn` applied to
    each doc's sidecar.  Sorted by operator ASCENDING (most-confident first)."""
    n = len(docs)
    if n == 0:
        return []
    scored = [(op_fn(d["sidecar"]), d["acc"]) for d in docs]
    scored.sort(key=lambda t: t[0])
    out: List[Tuple[float, float]] = []
    cum = 0.0
    for k in range(1, n + 1):
        cum += scored[k - 1][1]
        out.append((k / n, cum / k))
    return out


def _aurcc_from_curve(curve: List[Tuple[float, float]]) -> float:
    if len(curve) < 2:
        return float("nan")
    area = 0.0
    for i in range(1, len(curve)):
        x0, a0 = curve[i - 1]
        x1, a1 = curve[i]
        r0 = 1.0 - a0; r1 = 1.0 - a1
        area += 0.5 * (r0 + r1) * (x1 - x0)
    return area


def _aurcc_for_op(docs: List[dict], op_name: str) -> float:
    curve = _rc_curve_op(docs, OPERATORS[op_name])
    return _aurcc_from_curve(curve)


# ---------------- paired bootstrap ΔAURCC(op − entropy) ---------------- #

def _paired_bootstrap_delta(docs: List[dict],
                            op_name: str,
                            ref_name: str = "entropy",
                            n_boot: int = 2000,
                            seed: int = 0) -> Dict[str, float]:
    """Return dict with mean / CI95 / two-sided p-value for
    ΔAURCC = AURCC(op_name) − AURCC(ref_name).  Both metrics are
    recomputed on the SAME bootstrap resample of doc indices.

    The reported p-value is the standard bootstrap achieved significance
    level for H0: Δ = 0, two-sided, computed as
        p = 2 * min(P(Δ* ≤ 0), P(Δ* ≥ 0))
    on the bootstrap distribution of Δ*.
    """
    rng = random.Random(seed)
    n = len(docs)
    if n < 10:
        return {"delta_mean": float("nan"), "lo95": float("nan"),
                "hi95": float("nan"), "p_two_sided": float("nan"),
                "se": float("nan")}
    deltas: List[float] = []
    op_a = OPERATORS[op_name]
    op_b = OPERATORS[ref_name]
    for _ in range(n_boot):
        idx = [rng.randrange(n) for _ in range(n)]
        resampled = [docs[i] for i in idx]
        a = _aurcc_from_curve(_rc_curve_op(resampled, op_a))
        b = _aurcc_from_curve(_rc_curve_op(resampled, op_b))
        deltas.append(a - b)
    deltas.sort()
    mean = sum(deltas) / n_boot
    lo = deltas[int(0.025 * n_boot)]
    hi = deltas[int(0.975 * n_boot)]
    se = statistics.stdev(deltas) if n_boot > 1 else float("nan")
    # two-sided bootstrap p-value
    n_le0 = sum(1 for d in deltas if d <= 0)
    n_ge0 = sum(1 for d in deltas if d >= 0)
    p = 2.0 * min(n_le0, n_ge0) / n_boot
    p = max(p, 1.0 / n_boot)  # floor
    return {"delta_mean": mean, "lo95": lo, "hi95": hi,
            "p_two_sided": p, "se": se}


# ---------------- driver ---------------- #

BONFERRONI_ALPHA = 0.05 / BONFERRONI_N  # pre-registered 0.005


def run_for_task(task: str, results_root: str,
                 ref: str = "entropy",
                 n_boot: int = 2000) -> Dict[str, Any]:
    docs = _per_doc_rows(task, results_root)
    n = len(docs)
    overall_acc = sum(d["acc"] for d in docs) / max(1, n)

    rows: List[Dict[str, Any]] = []
    for op_name in OPERATORS:
        aurcc = _aurcc_for_op(docs, op_name)
        auacc = 1.0 - aurcc  # by construction on [0,1] acc
        stat = _paired_bootstrap_delta(docs, op_name, ref_name=ref, n_boot=n_boot)
        rows.append({
            "operator": op_name,
            "AURCC": aurcc,
            "AUACC": auacc,
            "delta_mean": stat["delta_mean"],
            "CI95_lo": stat["lo95"],
            "CI95_hi": stat["hi95"],
            "se": stat["se"],
            "p": stat["p_two_sided"],
        })
    return {
        "task": task,
        "n": n,
        "overall_acc": overall_acc,
        "ref": ref,
        "alpha_bonferroni": BONFERRONI_ALPHA,
        "rows": rows,
    }


def _fmt_table(res: Dict[str, Any]) -> str:
    out = []
    out.append(f"### {res['task']} — n={res['n']}, overall_acc={res['overall_acc']:.4f}, "
               f"paired bootstrap vs `{res['ref']}`, α_Bonferroni={res['alpha_bonferroni']:.4f}\n")
    out.append("| operator | AURCC ↓ | ΔAURCC vs ref | CI95 | p (2-sided) | significant? |")
    out.append("|---|---:|---:|---|---:|:---:|")
    for r in res["rows"]:
        d = r["delta_mean"]; lo = r["CI95_lo"]; hi = r["CI95_hi"]; p = r["p"]
        sig = "yes" if (p < res["alpha_bonferroni"] and d < 0) else (
              "HURTS" if (p < res["alpha_bonferroni"] and d > 0) else "")
        out.append(f"| `{r['operator']}` | {r['AURCC']:.4f} | {d:+.4f} "
                   f"| [{lo:+.4f},{hi:+.4f}] | {p:.4f} | {sig} |")
    return "\n".join(out) + "\n"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--results", default="benchmarks/v103/results",
                    help="sidecar / lm-eval root (default: v103's for prelim runs)")
    ap.add_argument("--tasks",  default="arc_easy,arc_challenge,truthfulqa_mc2")
    ap.add_argument("--ref",    default="entropy")
    ap.add_argument("--n-boot", type=int, default=2000)
    ap.add_argument("--out-dir", default="benchmarks/v104/results")
    args = ap.parse_args()

    tasks = [t.strip() for t in args.tasks.split(",") if t.strip()]
    os.makedirs(args.out_dir, exist_ok=True)

    all_res: List[Dict[str, Any]] = []
    md_chunks: List[str] = []
    md_chunks.append("# v104 operator comparison — paired bootstrap ΔAURCC vs entropy\n")
    md_chunks.append(f"Bonferroni α = 0.05 / {BONFERRONI_N} = {BONFERRONI_ALPHA:.4f} (ten σ candidates).\n")
    md_chunks.append(f"n_boot = {args.n_boot}; sidecar root = `{args.results}`\n")

    for task in tasks:
        try:
            res = run_for_task(task, args.results, ref=args.ref, n_boot=args.n_boot)
        except FileNotFoundError as e:
            md_chunks.append(f"\n### {task}\nSKIP — {e}\n")
            continue
        all_res.append(res)
        block = _fmt_table(res)
        md_chunks.append("\n" + block)
        print(block)

    json_path = os.path.join(args.out_dir, "operator_comparison.json")
    md_path   = os.path.join(args.out_dir, "operator_comparison.md")
    with open(json_path, "w") as fh:
        json.dump({"sidecar_root": args.results,
                   "n_boot": args.n_boot,
                   "alpha_bonferroni": BONFERRONI_ALPHA,
                   "tasks": all_res}, fh, indent=2, sort_keys=True)
    with open(md_path, "w") as fh:
        fh.write("".join(md_chunks))
    print(f"wrote {json_path}")
    print(f"wrote {md_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
