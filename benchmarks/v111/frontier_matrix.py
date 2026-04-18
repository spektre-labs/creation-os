# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2026 Spektre Labs / Lauri Rainio
"""
v111 — frontier parity matrix.

Re-analyses v103 / v104 sidecar JSONLs (and any new v111 sidecars) with
the six pre-registered v111 signals, producing:

    benchmarks/v111/results/frontier_matrix.json   full numeric dump
    benchmarks/v111/results/frontier_matrix.md     human-readable table
    benchmarks/v111/results/rc_curves.json         per-task, per-signal RC points

Each (task, σ-signal) pair gets a paired-bootstrap ΔAURCC vs the
entropy baseline at n_boot=2000 with Bonferroni α = 0.05 / 24 = 0.00208.

Tasks that lack a sidecar are emitted as PENDING rows in the markdown and
omitted from the JSON; the matrix renders in any state.
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

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(REPO, "benchmarks", "v103"))
sys.path.insert(0, os.path.join(REPO, "benchmarks", "v104"))

from frontier_signals import SIGNALS, BONFERRONI_N, TASK_FAMILIES  # noqa: E402

# v103 loaders: one row per document (lm-eval row + σ sidecar row).
try:
    from compute_rc_metrics import (  # type: ignore  # noqa: E402
        _load_lm_eval_samples,
        _load_sigma_sidecar,
    )
except Exception as e:
    print(f"v111: cannot import v103 loaders: {e}", file=sys.stderr)
    raise

BONFERRONI_ALPHA = 0.05 / BONFERRONI_N


# ------------------------------ RC math ----------------------------------- #

def _per_doc_rows(task: str, results_root: str) -> List[dict]:
    """Return [{'acc', 'sidecar'}, ...] joining lm-eval samples with the
    σ-sidecar row of the argmax-loglikelihood candidate per document."""
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
            raise KeyError(f"v111: σ sidecar missing doc {doc_id} in {task}")
        s = dict(s)
        s["doc_index"] = doc_id
        s["cand_index"] = top1
        out.append({"acc": float(row.get("acc", 0.0)), "sidecar": s})
    return out


def _rc_curve(docs: List[dict], signal_fn) -> List[Tuple[float, float]]:
    """Rank docs by signal ASCENDING (most confident first); emit
    (coverage, conditional-accuracy) points."""
    n = len(docs)
    if n == 0:
        return []
    scored = [(signal_fn(d["sidecar"]), d["acc"]) for d in docs]
    scored.sort(key=lambda t: t[0])
    cum = 0.0
    out: List[Tuple[float, float]] = []
    for k in range(1, n + 1):
        cum += scored[k - 1][1]
        out.append((k / n, cum / k))
    return out


def _aurcc(curve: List[Tuple[float, float]]) -> float:
    if len(curve) < 2:
        return float("nan")
    area = 0.0
    for i in range(1, len(curve)):
        x0, a0 = curve[i - 1]
        x1, a1 = curve[i]
        r0 = 1.0 - a0
        r1 = 1.0 - a1
        area += 0.5 * (r0 + r1) * (x1 - x0)
    return area


def _coverage_at_acc(curve: List[Tuple[float, float]], target: float) -> float:
    """Max coverage such that conditional-accuracy ≥ target."""
    best = 0.0
    for cov, acc in curve:
        if acc >= target and cov > best:
            best = cov
    return best


def _paired_bootstrap(docs: List[dict],
                      sig_name: str,
                      ref_name: str = "entropy",
                      n_boot: int = 2000,
                      seed: int = 0) -> Dict[str, float]:
    rng = random.Random(seed)
    n = len(docs)
    if n < 10:
        return {"delta_mean": float("nan"), "lo95": float("nan"),
                "hi95": float("nan"), "p_two_sided": float("nan"),
                "se": float("nan")}
    deltas: List[float] = []
    a_fn = SIGNALS[sig_name]
    b_fn = SIGNALS[ref_name]
    for _ in range(n_boot):
        idx = [rng.randrange(n) for _ in range(n)]
        rs = [docs[i] for i in idx]
        a = _aurcc(_rc_curve(rs, a_fn))
        b = _aurcc(_rc_curve(rs, b_fn))
        deltas.append(a - b)
    deltas.sort()
    mean = sum(deltas) / n_boot
    lo = deltas[int(0.025 * n_boot)]
    hi = deltas[int(0.975 * n_boot)]
    se = statistics.stdev(deltas) if n_boot > 1 else float("nan")
    n_le0 = sum(1 for d in deltas if d <= 0)
    n_ge0 = sum(1 for d in deltas if d >= 0)
    p = 2.0 * min(n_le0, n_ge0) / n_boot
    p = max(p, 1.0 / n_boot)
    return {"delta_mean": mean, "lo95": lo, "hi95": hi,
            "p_two_sided": p, "se": se}


# ------------------------------ driver ----------------------------------- #

def _analyse_task(task: str, results_root: str, n_boot: int) -> Dict[str, Any]:
    docs = _per_doc_rows(task, results_root)
    n = len(docs)
    overall_acc = sum(d["acc"] for d in docs) / max(1, n)
    sig_rows: List[Dict[str, Any]] = []
    curves: Dict[str, List[Tuple[float, float]]] = {}
    for sig_name in SIGNALS:
        fn = SIGNALS[sig_name]
        curve = _rc_curve(docs, fn)
        aurcc = _aurcc(curve)
        cov_at_90 = _coverage_at_acc(curve, 0.90)
        cov_at_95 = _coverage_at_acc(curve, 0.95)
        stat = _paired_bootstrap(docs, sig_name, "entropy", n_boot=n_boot)
        sig_rows.append({
            "signal": sig_name,
            "AURCC": aurcc,
            "AUACC": 1.0 - aurcc,
            "coverage_at_acc90": cov_at_90,
            "coverage_at_acc95": cov_at_95,
            "delta_vs_entropy": stat["delta_mean"],
            "CI95_lo": stat["lo95"],
            "CI95_hi": stat["hi95"],
            "p_two_sided": stat["p_two_sided"],
            "bonferroni_significant":
                (stat["p_two_sided"] < BONFERRONI_ALPHA
                 and stat["delta_mean"] < 0),
        })
        curves[sig_name] = curve
    return {
        "task": task,
        "n": n,
        "overall_acc": overall_acc,
        "rows": sig_rows,
        "rc_curves": curves,
    }


def _fmt_table(task_res: Dict[str, Any]) -> str:
    out: List[str] = []
    out.append(f"### {task_res['task']} — n={task_res['n']} · acc={task_res['overall_acc']:.4f}")
    out.append("")
    out.append("| signal | AURCC ↓ | ΔAURCC vs entropy | CI95 | p | cov@acc≥0.90 | cov@acc≥0.95 | Bonferroni |")
    out.append("|---|---:|---:|---|---:|---:|---:|:---:|")
    for r in task_res["rows"]:
        d = r["delta_vs_entropy"]; lo = r["CI95_lo"]; hi = r["CI95_hi"]
        sig = "**yes**" if r["bonferroni_significant"] else (
            "HURTS" if r["p_two_sided"] < BONFERRONI_ALPHA and d > 0 else ""
        )
        out.append(
            f"| `{r['signal']}` | {r['AURCC']:.4f} | {d:+.4f} | "
            f"[{lo:+.4f},{hi:+.4f}] | {r['p_two_sided']:.4f} | "
            f"{r['coverage_at_acc90']:.3f} | {r['coverage_at_acc95']:.3f} | {sig} |"
        )
    out.append("")
    return "\n".join(out) + "\n"


def _search_sidecar_root(task: str, candidate_roots: List[str]) -> str | None:
    """Return the first root under which both
    `results/samples_<task>_sigma.jsonl` (or the sidecar canonical name)
    and `results/lm_eval/<task>/` exist.  Returns None if nothing matches."""
    for root in candidate_roots:
        sidecar_p = os.path.join(root, f"samples_{task}_sigma.jsonl")
        lm_dir = os.path.join(root, "lm_eval", task)
        if os.path.isfile(sidecar_p) and os.path.isdir(lm_dir):
            return root
    return None


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--tasks", default=",".join(TASK_FAMILIES),
                    help="comma-sep task list (default: %(default)s)")
    ap.add_argument("--roots",
                    default="benchmarks/v104/n5000_results,"
                            "benchmarks/v111/results,"
                            "benchmarks/v103/results",
                    help="comma-sep sidecar roots to search, in priority order")
    ap.add_argument("--n-boot", type=int, default=2000)
    ap.add_argument("--out-dir", default="benchmarks/v111/results")
    args = ap.parse_args()

    tasks = [t.strip() for t in args.tasks.split(",") if t.strip()]
    roots = [os.path.join(REPO, r.strip()) for r in args.roots.split(",") if r.strip()]
    os.makedirs(args.out_dir, exist_ok=True)

    md: List[str] = []
    md.append("# v111 frontier parity matrix\n")
    md.append(f"- gating signals (6): {', '.join(list(SIGNALS.keys()))}")
    md.append(f"- task families (4): {', '.join(TASK_FAMILIES)}")
    md.append(f"- Bonferroni N = {BONFERRONI_N}, α = 0.05 / {BONFERRONI_N} = "
              f"{BONFERRONI_ALPHA:.5f}")
    md.append(f"- n_boot = {args.n_boot}")
    md.append("")
    md.append("Baseline = `entropy` (channel 0 of σ profile). "
              "AURCC lower = better selective-prediction calibration. "
              "`Bonferroni` column marks signals that beat the baseline "
              "below the family-wise α.\n")

    out_results: List[Dict[str, Any]] = []
    rc_dump: Dict[str, Dict[str, List[List[float]]]] = {}

    for task in tasks:
        root = _search_sidecar_root(task, roots)
        if root is None:
            md.append(f"### {task} — PENDING")
            md.append("")
            md.append("No sidecar found.  To populate this row run "
                      f"`bash benchmarks/v111/run_matrix.sh {task}` "
                      "(needs BitNet weights + .venv-bitnet + lm-eval).")
            md.append("")
            continue
        try:
            res = _analyse_task(task, root, args.n_boot)
        except (FileNotFoundError, KeyError) as e:
            md.append(f"### {task} — ERROR")
            md.append(f"`{e}`")
            md.append("")
            continue
        out_results.append({
            "task": res["task"],
            "n": res["n"],
            "overall_acc": res["overall_acc"],
            "sidecar_root": root,
            "rows": res["rows"],
        })
        rc_dump[task] = {
            sig: [[float(c), float(a)] for (c, a) in curve]
            for sig, curve in res["rc_curves"].items()
        }
        md.append(_fmt_table(res))

    summary_best: Dict[str, Any] = {}
    if out_results:
        md.append("### Cross-task best signal (Bonferroni wins count)")
        md.append("")
        md.append("| signal | Bonferroni wins | mean ΔAURCC | tasks beaten |")
        md.append("|---|---:|---:|---|")
        per_sig_wins: Dict[str, List[str]] = {s: [] for s in SIGNALS}
        per_sig_deltas: Dict[str, List[float]] = {s: [] for s in SIGNALS}
        for r in out_results:
            for row in r["rows"]:
                if row["bonferroni_significant"]:
                    per_sig_wins[row["signal"]].append(r["task"])
                if not math.isnan(row["delta_vs_entropy"]):
                    per_sig_deltas[row["signal"]].append(row["delta_vs_entropy"])
        for sig in SIGNALS:
            if sig == "entropy":
                continue
            wins = per_sig_wins[sig]
            deltas = per_sig_deltas[sig]
            md.append(
                f"| `{sig}` | {len(wins)} | "
                f"{(sum(deltas)/len(deltas)) if deltas else float('nan'):+.4f} | "
                f"{', '.join(wins) if wins else '—'} |"
            )
            summary_best[sig] = {"wins": wins,
                                 "mean_delta": (sum(deltas)/len(deltas)) if deltas else None}
        md.append("")

    json_path = os.path.join(args.out_dir, "frontier_matrix.json")
    md_path = os.path.join(args.out_dir, "frontier_matrix.md")
    rc_path = os.path.join(args.out_dir, "rc_curves.json")
    payload = {
        "bonferroni_N": BONFERRONI_N,
        "alpha_bonferroni": BONFERRONI_ALPHA,
        "n_boot": args.n_boot,
        "signals": list(SIGNALS.keys()),
        "tasks": out_results,
        "cross_task_summary": summary_best,
    }
    with open(json_path, "w") as fh:
        json.dump(payload, fh, indent=2, sort_keys=True)
    with open(md_path, "w") as fh:
        fh.write("\n".join(md))
    with open(rc_path, "w") as fh:
        json.dump(rc_dump, fh, separators=(",", ":"))
    print(f"wrote {json_path}")
    print(f"wrote {md_path}")
    print(f"wrote {rc_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
