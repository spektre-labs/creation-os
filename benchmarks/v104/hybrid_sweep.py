# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2026 Spektre Labs / Lauri Rainio
"""
v104 — α-sweep for an entropy × σ_second hybrid.

    hybrid_α(row) = α · σ_second(row) + (1 - α) · entropy(row)

where `σ_second` defaults to `sigma_max_token` (the v103 secondary
winner) but can be any other operator name from `operators.py`.
α ∈ {0.0, 0.1, ..., 1.0}.

For every α we compute AURCC + paired bootstrap ΔAURCC vs. entropy, and
emit a table so the optimal α can be read off the curve.

This is the pre-registered Phase 5 of docs/v104/OPERATOR_SEARCH.md.
If the optimal α is strictly interior (0 < α* < 1) and the resulting
AURCC beats entropy at α_Bonferroni, that confirms H3: σ and entropy
carry independent information.
"""

from __future__ import annotations

import argparse
import json
import os
import random
import statistics
import sys
from typing import Any, Dict, List

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "v103"))

from operators import OPERATORS  # noqa: E402
from compute_operator_rcc import (  # noqa: E402
    _per_doc_rows, _aurcc_from_curve, _rc_curve_op,
)


BONFERRONI_N = 11  # 11 α-points (0.0..1.0, step 0.1)
BONFERRONI_ALPHA = 0.05 / BONFERRONI_N


def _make_hybrid(sec_name: str, alpha: float):
    sec_op = OPERATORS[sec_name]
    ent_op = OPERATORS["entropy"]
    def _op(row: dict) -> float:
        return alpha * sec_op(row) + (1.0 - alpha) * ent_op(row)
    return _op


def _paired_delta(docs, op_a, op_b, n_boot: int, seed: int) -> Dict[str, float]:
    rng = random.Random(seed)
    n = len(docs)
    ds: List[float] = []
    for _ in range(n_boot):
        idx = [rng.randrange(n) for _ in range(n)]
        resampled = [docs[i] for i in idx]
        ds.append(_aurcc_from_curve(_rc_curve_op(resampled, op_a))
                  - _aurcc_from_curve(_rc_curve_op(resampled, op_b)))
    ds.sort()
    mean = sum(ds) / n_boot
    lo = ds[int(0.025 * n_boot)]
    hi = ds[int(0.975 * n_boot)]
    n_le0 = sum(1 for d in ds if d <= 0)
    n_ge0 = sum(1 for d in ds if d >= 0)
    p = 2.0 * min(n_le0, n_ge0) / n_boot
    p = max(p, 1.0 / n_boot)
    return {"delta_mean": mean, "lo95": lo, "hi95": hi,
            "p_two_sided": p,
            "se": statistics.stdev(ds) if n_boot > 1 else float("nan")}


def run_for_task(task: str, results_root: str,
                 second: str = "sigma_max_token",
                 alphas: List[float] = None,
                 n_boot: int = 2000) -> Dict[str, Any]:
    if alphas is None:
        alphas = [round(i * 0.1, 2) for i in range(11)]
    docs = _per_doc_rows(task, results_root)
    n = len(docs)
    overall_acc = sum(d["acc"] for d in docs) / max(1, n)

    ent_op = OPERATORS["entropy"]
    rows: List[Dict[str, Any]] = []
    for i, a in enumerate(alphas):
        hyb = _make_hybrid(second, a)
        aurcc = _aurcc_from_curve(_rc_curve_op(docs, hyb))
        stat = _paired_delta(docs, hyb, ent_op, n_boot, seed=int(a * 1000) + 7)
        rows.append({
            "alpha": a,
            "AURCC": aurcc,
            "delta_vs_entropy": stat["delta_mean"],
            "CI95_lo": stat["lo95"],
            "CI95_hi": stat["hi95"],
            "p": stat["p_two_sided"],
        })

    # best α (most negative delta)
    best = min(rows, key=lambda r: r["AURCC"])
    return {
        "task": task,
        "n": n,
        "overall_acc": overall_acc,
        "second": second,
        "alpha_bonferroni": BONFERRONI_ALPHA,
        "best_alpha": best["alpha"],
        "best_AURCC": best["AURCC"],
        "best_delta": best["delta_vs_entropy"],
        "best_p": best["p"],
        "rows": rows,
    }


def _fmt_table(res: Dict[str, Any]) -> str:
    out = []
    out.append(f"### {res['task']} — n={res['n']}, overall_acc={res['overall_acc']:.4f}, "
               f"second=`{res['second']}`, α_Bonferroni={res['alpha_bonferroni']:.4f} (11 α-points)\n")
    out.append(f"Best α* = {res['best_alpha']}, AURCC = {res['best_AURCC']:.4f}, "
               f"Δ = {res['best_delta']:+.4f}, p = {res['best_p']:.4f}\n")
    out.append("| α | AURCC ↓ | Δ vs entropy | CI95 | p | sig? |")
    out.append("|---:|---:|---:|---|---:|:---:|")
    for r in res["rows"]:
        sig = "yes" if (r["p"] < res["alpha_bonferroni"] and r["delta_vs_entropy"] < 0) else (
              "HURTS" if (r["p"] < res["alpha_bonferroni"] and r["delta_vs_entropy"] > 0) else "")
        out.append(f"| {r['alpha']:.1f} | {r['AURCC']:.4f} | {r['delta_vs_entropy']:+.4f} "
                   f"| [{r['CI95_lo']:+.4f},{r['CI95_hi']:+.4f}] | {r['p']:.4f} | {sig} |")
    return "\n".join(out) + "\n"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--results", default="benchmarks/v103/results")
    ap.add_argument("--tasks",   default="arc_easy,arc_challenge,truthfulqa_mc2")
    ap.add_argument("--second",  default="sigma_max_token",
                    help="operator name from operators.py to hybrid with entropy")
    ap.add_argument("--n-boot",  type=int, default=2000)
    ap.add_argument("--out-dir", default="benchmarks/v104/results")
    args = ap.parse_args()

    tasks = [t.strip() for t in args.tasks.split(",") if t.strip()]
    os.makedirs(args.out_dir, exist_ok=True)

    all_res: List[Dict[str, Any]] = []
    md_chunks: List[str] = []
    md_chunks.append(f"# v104 — α-sweep: hybrid(α) = α·{args.second} + (1-α)·entropy\n\n")
    md_chunks.append(f"Bonferroni α = 0.05 / {BONFERRONI_N} = {BONFERRONI_ALPHA:.4f} "
                     f"(11 α-points per task)\n")
    md_chunks.append(f"n_boot = {args.n_boot}; sidecar root = `{args.results}`\n")

    for task in tasks:
        try:
            res = run_for_task(task, args.results, second=args.second, n_boot=args.n_boot)
        except FileNotFoundError as e:
            md_chunks.append(f"\n### {task}\nSKIP — {e}\n")
            continue
        all_res.append(res)
        block = _fmt_table(res)
        md_chunks.append("\n" + block)
        print(block)

    json_path = os.path.join(args.out_dir, f"hybrid_sweep_{args.second}.json")
    md_path   = os.path.join(args.out_dir, f"hybrid_sweep_{args.second}.md")
    with open(json_path, "w") as fh:
        json.dump({"sidecar_root": args.results, "n_boot": args.n_boot,
                   "second": args.second,
                   "alpha_bonferroni": BONFERRONI_ALPHA,
                   "tasks": all_res}, fh, indent=2, sort_keys=True)
    with open(md_path, "w") as fh:
        fh.write("".join(md_chunks))
    print(f"wrote {json_path}")
    print(f"wrote {md_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
