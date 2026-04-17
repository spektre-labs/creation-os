# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2026 Spektre Labs / Lauri Rainio
"""
v104 — per-channel selective-prediction diagnostic.

For every individual σ channel (entropy_norm, margin, top_k_mass,
tail_mass, logit_spread, p_max, n_effective, logit_std) compute the
AURCC and a paired bootstrap ΔAURCC vs. channel 0 (entropy_norm).  This
isolates which of the eight channels actually carries selective-
prediction signal vs. which are redundant or destructive.

Output: benchmarks/v104/results/channel_ranking.md, channel_ranking.json
"""

from __future__ import annotations

import argparse
import json
import os
import sys
from typing import Any, Dict, List

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "v103"))

from compute_operator_rcc import _per_doc_rows, _aurcc_from_curve, _rc_curve_op  # noqa: E402
from operators import OPERATORS  # noqa: E402

import random
import statistics


CHANNEL_NAMES = [
    "entropy_norm",   # 0
    "margin",         # 1
    "top_k_mass",     # 2
    "tail_mass",      # 3
    "logit_spread",   # 4
    "p_max",          # 5
    "n_effective",    # 6
    "logit_std",      # 7
]


def _make_channel_op(idx: int):
    def _op(row: dict) -> float:
        return float(row["sigma_profile"][idx])
    return _op


def _paired_delta_vs_ref(docs, op_a, op_b, n_boot, seed: int) -> Dict[str, float]:
    rng = random.Random(seed)
    n = len(docs)
    ds: List[float] = []
    for _ in range(n_boot):
        idx = [rng.randrange(n) for _ in range(n)]
        resampled = [docs[i] for i in idx]
        a = _aurcc_from_curve(_rc_curve_op(resampled, op_a))
        b = _aurcc_from_curve(_rc_curve_op(resampled, op_b))
        ds.append(a - b)
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


BONFERRONI_N_CHANNELS = 7  # channels 1..7 vs channel 0 (entropy)
BONFERRONI_ALPHA = 0.05 / BONFERRONI_N_CHANNELS


def run_for_task(task: str, results_root: str, n_boot: int = 2000) -> Dict[str, Any]:
    docs = _per_doc_rows(task, results_root)
    n = len(docs)
    overall_acc = sum(d["acc"] for d in docs) / max(1, n)

    ref_op = _make_channel_op(0)
    rows: List[Dict[str, Any]] = []
    for idx, name in enumerate(CHANNEL_NAMES):
        op = _make_channel_op(idx)
        aurcc = _aurcc_from_curve(_rc_curve_op(docs, op))
        stat = {"delta_mean": 0.0, "lo95": 0.0, "hi95": 0.0,
                "p_two_sided": 2.0, "se": 0.0} if idx == 0 else \
               _paired_delta_vs_ref(docs, op, ref_op, n_boot, seed=idx)
        rows.append({
            "channel_idx": idx,
            "channel_name": name,
            "AURCC": aurcc,
            "delta_vs_entropy": stat["delta_mean"],
            "CI95_lo": stat["lo95"],
            "CI95_hi": stat["hi95"],
            "p": stat["p_two_sided"],
        })
    return {
        "task": task,
        "n": n,
        "overall_acc": overall_acc,
        "alpha_bonferroni": BONFERRONI_ALPHA,
        "rows": rows,
    }


def _fmt_table(res: Dict[str, Any]) -> str:
    out = []
    out.append(f"### {res['task']} — n={res['n']}, overall_acc={res['overall_acc']:.4f}, "
               f"α_Bonferroni={res['alpha_bonferroni']:.4f} (7 channels vs entropy_norm)\n")
    out.append("| idx | channel | AURCC ↓ | Δ vs entropy | CI95 | p | sig? |")
    out.append("|---:|---|---:|---:|---|---:|:---:|")
    for r in res["rows"]:
        sig = "yes" if (r["p"] < res["alpha_bonferroni"] and r["delta_vs_entropy"] < 0) else (
              "HURTS" if (r["p"] < res["alpha_bonferroni"] and r["delta_vs_entropy"] > 0) else "")
        out.append(f"| {r['channel_idx']} | `{r['channel_name']}` | {r['AURCC']:.4f} "
                   f"| {r['delta_vs_entropy']:+.4f} "
                   f"| [{r['CI95_lo']:+.4f},{r['CI95_hi']:+.4f}] "
                   f"| {r['p']:.4f} | {sig} |")
    return "\n".join(out) + "\n"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--results", default="benchmarks/v103/results")
    ap.add_argument("--tasks",  default="arc_easy,arc_challenge,truthfulqa_mc2")
    ap.add_argument("--n-boot", type=int, default=2000)
    ap.add_argument("--out-dir", default="benchmarks/v104/results")
    args = ap.parse_args()

    tasks = [t.strip() for t in args.tasks.split(",") if t.strip()]
    os.makedirs(args.out_dir, exist_ok=True)

    all_res: List[Dict[str, Any]] = []
    md_chunks: List[str] = []
    md_chunks.append("# v104 — per-channel selective-prediction diagnostic\n\n")
    md_chunks.append(f"Bonferroni α = 0.05 / {BONFERRONI_N_CHANNELS} = {BONFERRONI_ALPHA:.4f}\n")
    md_chunks.append(f"n_boot = {args.n_boot}; sidecar root = `{args.results}`\n")

    for task in tasks:
        try:
            res = run_for_task(task, args.results, n_boot=args.n_boot)
        except FileNotFoundError as e:
            md_chunks.append(f"\n### {task}\nSKIP — {e}\n")
            continue
        all_res.append(res)
        block = _fmt_table(res)
        md_chunks.append("\n" + block)
        print(block)

    json_path = os.path.join(args.out_dir, "channel_ranking.json")
    md_path   = os.path.join(args.out_dir, "channel_ranking.md")
    with open(json_path, "w") as fh:
        json.dump({"sidecar_root": args.results, "n_boot": args.n_boot,
                   "alpha_bonferroni": BONFERRONI_ALPHA,
                   "tasks": all_res}, fh, indent=2, sort_keys=True)
    with open(md_path, "w") as fh:
        fh.write("".join(md_chunks))
    print(f"wrote {json_path}")
    print(f"wrote {md_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
