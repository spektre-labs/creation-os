#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2026 Spektre Labs / Lauri Rainio
"""
v111.2 — adaptive / composite σ exploration (post-hoc).

Reuses `frontier_matrix._per_doc_rows` to load the same v103/v104/v111
sidecars, then scores every task against three additional EXPLORATORY
signals defined in `adaptive_signals.py`:

    sigma_composite_max   = max(σ_max_token, entropy)
    sigma_composite_mean  = 0.5·(σ_max_token + entropy)
    sigma_task_adaptive   = per-task pre-specified routing

Output files
    benchmarks/v111/results/frontier_matrix_adaptive.json
    benchmarks/v111/results/frontier_matrix_adaptive.md
    benchmarks/v111/results/rc_curves_adaptive.json

Every row is labelled `post_hoc = true` in the JSON and is rendered
under a bolded "POST-HOC EXPLORATION — NOT PRE-REGISTERED" banner in
the markdown.  Separate Bonferroni N = 12 (3 signals × 4 tasks), and
α_fw = 0.05 / 12 = 0.00417.

CLAIM_DISCIPLINE: numbers here may be reported, but NEVER as part of
the pre-registered v111 claim.  They are a v111.2 directional signal,
not a confirmation.
"""

from __future__ import annotations

import argparse
import json
import os
import random
import statistics
import sys
from typing import Any, Dict, List, Tuple

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(REPO, "benchmarks", "v103"))
sys.path.insert(0, os.path.join(REPO, "benchmarks", "v104"))

from adaptive_signals import (   # noqa: E402
    ADAPTIVE_SIGNALS,
    ADAPTIVE_TASK_ROUTED,
    ADAPTIVE_BONFERRONI_N,
    adaptive_signal_for,
    _dispatch,
)
from frontier_signals import op_entropy, TASK_FAMILIES  # noqa: E402
from frontier_matrix import (   # noqa: E402
    _per_doc_rows,
    _aurcc,
    _coverage_at_acc,
    _search_sidecar_root,
)

ADAPTIVE_ALPHA = 0.05 / ADAPTIVE_BONFERRONI_N


def _rc_curve_for_fn(docs: List[dict], fn) -> List[Tuple[float, float]]:
    """Identical to frontier_matrix._rc_curve but takes any scalar fn."""
    n = len(docs)
    if n == 0:
        return []
    scored = [(float(fn(d["sidecar"])), d["acc"]) for d in docs]
    scored.sort(key=lambda t: t[0])
    cum = 0.0
    out: List[Tuple[float, float]] = []
    for k in range(1, n + 1):
        cum += scored[k - 1][1]
        out.append((k / n, cum / k))
    return out


def _paired_bootstrap_fn(docs: List[dict], fn_a, fn_b,
                         n_boot: int, seed: int) -> Dict[str, float]:
    rng = random.Random(seed)
    n = len(docs)
    if n < 10:
        return {"delta_mean": float("nan"), "lo95": float("nan"),
                "hi95": float("nan"), "p_two_sided": float("nan")}
    deltas: List[float] = []
    for _ in range(n_boot):
        idx = [rng.randrange(n) for _ in range(n)]
        rs = [docs[i] for i in idx]
        a = _aurcc(_rc_curve_for_fn(rs, fn_a))
        b = _aurcc(_rc_curve_for_fn(rs, fn_b))
        deltas.append(a - b)
    deltas.sort()
    mean = sum(deltas) / n_boot
    lo = deltas[int(0.025 * n_boot)]
    hi = deltas[int(0.975 * n_boot)]
    n_le0 = sum(1 for d in deltas if d <= 0)
    n_ge0 = sum(1 for d in deltas if d >= 0)
    p = 2.0 * min(n_le0, n_ge0) / n_boot
    p = max(p, 1.0 / n_boot)
    return {"delta_mean": mean, "lo95": lo, "hi95": hi, "p_two_sided": p}


def _analyse_task(task: str, results_root: str, n_boot: int) -> Dict[str, Any]:
    docs = _per_doc_rows(task, results_root)
    n = len(docs)
    overall_acc = sum(d["acc"] for d in docs) / max(1, n)

    rows: List[Dict[str, Any]] = []
    curves: Dict[str, List[Tuple[float, float]]] = {}

    # Entropy baseline curve for reference.
    curves["entropy"] = _rc_curve_for_fn(docs, lambda r: op_entropy(r))
    aurcc_ent = _aurcc(curves["entropy"])

    for name, fn in ADAPTIVE_SIGNALS.items():
        curve = _rc_curve_for_fn(docs, fn)
        curves[name] = curve
        aurcc = _aurcc(curve)
        stat = _paired_bootstrap_fn(
            docs, fn, lambda r: op_entropy(r), n_boot=n_boot, seed=0)
        rows.append({
            "signal": name,
            "post_hoc": True,
            "AURCC": aurcc,
            "delta_vs_entropy": stat["delta_mean"],
            "CI95_lo": stat["lo95"],
            "CI95_hi": stat["hi95"],
            "p_two_sided": stat["p_two_sided"],
            "coverage_at_acc90": _coverage_at_acc(curve, 0.90),
            "coverage_at_acc95": _coverage_at_acc(curve, 0.95),
            "bonferroni_significant":
                (stat["p_two_sided"] < ADAPTIVE_ALPHA
                 and stat["delta_mean"] < 0),
        })

    # Task-routed signal: binds task name into the closure.
    def routed_fn(r: dict, t: str = task) -> float:
        return _dispatch(t, r)
    curve_r = _rc_curve_for_fn(docs, routed_fn)
    curves[ADAPTIVE_TASK_ROUTED] = curve_r
    aurcc_r = _aurcc(curve_r)
    stat_r = _paired_bootstrap_fn(
        docs, routed_fn, lambda r: op_entropy(r), n_boot=n_boot, seed=0)
    rows.append({
        "signal": ADAPTIVE_TASK_ROUTED,
        "post_hoc": True,
        "AURCC": aurcc_r,
        "delta_vs_entropy": stat_r["delta_mean"],
        "CI95_lo": stat_r["lo95"],
        "CI95_hi": stat_r["hi95"],
        "p_two_sided": stat_r["p_two_sided"],
        "coverage_at_acc90": _coverage_at_acc(curve_r, 0.90),
        "coverage_at_acc95": _coverage_at_acc(curve_r, 0.95),
        "bonferroni_significant":
            (stat_r["p_two_sided"] < ADAPTIVE_ALPHA
             and stat_r["delta_mean"] < 0),
    })

    return {
        "task": task,
        "n": n,
        "overall_acc": overall_acc,
        "aurcc_entropy_baseline": aurcc_ent,
        "rows": rows,
        "rc_curves": curves,
    }


def _fmt_task(task_res: Dict[str, Any]) -> str:
    out: List[str] = []
    out.append(f"### {task_res['task']} — n={task_res['n']} · "
               f"acc={task_res['overall_acc']:.4f} · "
               f"entropy_AURCC={task_res['aurcc_entropy_baseline']:.4f}")
    out.append("")
    out.append("| signal | AURCC ↓ | ΔAURCC vs entropy | CI95 | p | "
               "cov@acc≥0.90 | cov@acc≥0.95 | Bonferroni (post-hoc) |")
    out.append("|---|---:|---:|---|---:|---:|---:|:---:|")
    for r in task_res["rows"]:
        d = r["delta_vs_entropy"]; lo = r["CI95_lo"]; hi = r["CI95_hi"]
        if r["bonferroni_significant"]:
            sig = "**yes**"
        elif (r["p_two_sided"] < ADAPTIVE_ALPHA and d > 0):
            sig = "HURTS"
        else:
            sig = ""
        out.append(
            f"| `{r['signal']}` | {r['AURCC']:.4f} | {d:+.4f} | "
            f"[{lo:+.4f},{hi:+.4f}] | {r['p_two_sided']:.4f} | "
            f"{r['coverage_at_acc90']:.3f} | {r['coverage_at_acc95']:.3f} | {sig} |"
        )
    out.append("")
    return "\n".join(out) + "\n"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--tasks", default=",".join(TASK_FAMILIES))
    ap.add_argument("--roots",
                    default=",".join([
                        "benchmarks/v111/results",
                        "benchmarks/v104/n5000_results",
                    ]))
    ap.add_argument("--n-boot", type=int, default=2000)
    ap.add_argument("--out-md",
                    default="benchmarks/v111/results/frontier_matrix_adaptive.md")
    ap.add_argument("--out-json",
                    default="benchmarks/v111/results/frontier_matrix_adaptive.json")
    ap.add_argument("--out-curves",
                    default="benchmarks/v111/results/rc_curves_adaptive.json")
    args = ap.parse_args()

    tasks = [t.strip() for t in args.tasks.split(",") if t.strip()]
    roots = [os.path.join(REPO, r) for r in args.roots.split(",") if r.strip()]

    matrix_tasks: List[Dict[str, Any]] = []
    all_curves: Dict[str, Dict[str, List[Tuple[float, float]]]] = {}
    for t in tasks:
        root = _search_sidecar_root(t, roots)
        if root is None:
            matrix_tasks.append({"task": t, "n": 0, "skipped": True})
            continue
        res = _analyse_task(t, root, args.n_boot)
        res["sidecar_root"] = root
        matrix_tasks.append({k: v for k, v in res.items() if k != "rc_curves"})
        all_curves[t] = {k: list(v) for k, v in res["rc_curves"].items()}

    md: List[str] = []
    md.append("# v111.2 adaptive / composite σ matrix")
    md.append("")
    md.append("> **POST-HOC EXPLORATION — NOT PRE-REGISTERED.**  The signals")
    md.append("> in this matrix (`sigma_composite_max`, `sigma_composite_mean`,")
    md.append("> `sigma_task_adaptive`) were designed _after_ the v111")
    md.append("> pre-registered matrix was analysed.  They may NOT be")
    md.append("> substituted into the pre-registered claim.  Report alongside,")
    md.append("> never in place of, `frontier_matrix.md`.")
    md.append("")
    md.append(f"- 3 post-hoc signals × 4 task families → Bonferroni N = "
              f"{ADAPTIVE_BONFERRONI_N}")
    md.append(f"- α (post-hoc family-wise) = 0.05 / "
              f"{ADAPTIVE_BONFERRONI_N} = {ADAPTIVE_ALPHA:.5f}")
    md.append(f"- n_boot = {args.n_boot}")
    md.append("")
    for tr in matrix_tasks:
        if tr.get("skipped"):
            md.append(f"### {tr['task']} — PENDING (sidecar absent)\n")
            continue
        md.append(_fmt_task(tr))

    # Cross-task summary: count post-hoc Bonferroni wins per signal.
    counts: Dict[str, Dict[str, Any]] = {}
    for tr in matrix_tasks:
        if tr.get("skipped"):
            continue
        for r in tr["rows"]:
            bucket = counts.setdefault(r["signal"], {
                "wins": 0, "delta_sum": 0.0, "n": 0, "tasks": []})
            bucket["n"] += 1
            bucket["delta_sum"] += r["delta_vs_entropy"]
            if r["bonferroni_significant"]:
                bucket["wins"] += 1
                bucket["tasks"].append(tr["task"])
    md.append("### Cross-task post-hoc summary\n")
    md.append("| signal | Bonferroni (post-hoc) wins | mean ΔAURCC | tasks beaten |")
    md.append("|---|---:|---:|---|")
    for sig, b in counts.items():
        mean_d = b["delta_sum"] / max(1, b["n"])
        tasks_str = ", ".join(b["tasks"]) if b["tasks"] else "—"
        md.append(f"| `{sig}` | {b['wins']} | {mean_d:+.4f} | {tasks_str} |")

    os.makedirs(os.path.dirname(args.out_md), exist_ok=True)
    with open(args.out_md, "w") as f:
        f.write("\n".join(md) + "\n")
    with open(args.out_json, "w") as f:
        json.dump({
            "post_hoc": True,
            "note": ("Exploratory signals designed AFTER the pre-registered "
                     "v111 matrix was analysed.  DO NOT merge into the "
                     "pre-registered claim."),
            "bonferroni_N": ADAPTIVE_BONFERRONI_N,
            "alpha_bonferroni": ADAPTIVE_ALPHA,
            "n_boot": args.n_boot,
            "signals": list(ADAPTIVE_SIGNALS) + [ADAPTIVE_TASK_ROUTED],
            "tasks": matrix_tasks,
            "cross_task_summary": counts,
        }, f, indent=2)
    with open(args.out_curves, "w") as f:
        json.dump(all_curves, f)

    print(f"wrote {args.out_md}")
    print(f"wrote {args.out_json}")
    print(f"wrote {args.out_curves}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
