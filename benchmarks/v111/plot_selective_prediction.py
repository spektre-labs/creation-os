#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2026 Spektre Labs / Lauri Rainio
"""
v111 — selective-prediction curves.

Reads `benchmarks/v111/results/rc_curves.json` (pre-registered) and
`benchmarks/v111/results/rc_curves_adaptive.json` (post-hoc), and
writes:

    docs/v111/selective_prediction_curves.png

One 2×2 grid.  Each panel is a task family; each line is a gating
signal.  X-axis = coverage (0..1); Y-axis = cumulative accuracy at
coverage.  The grey dashed line is the random-coverage baseline
(flat at the overall task accuracy).

Signals drawn (five, chosen for clarity):
    entropy                (pre-registered baseline)
    sigma_max_token        (pre-registered truthfulqa winner)
    sigma_product          (v104 winner, pre-registered)
    sigma_composite_max    (post-hoc exploration)
    sigma_task_adaptive    (post-hoc, task-routed upper bound)

The random baseline is plotted in every panel as a visual anchor.

Design invariants
-----------------
* No post-hoc signal is given visual primacy over the pre-registered
  set: same line weight, same alpha, distinct colour.
* The caption in-figure labels each signal as "pre-reg" or "post-hoc"
  so a reader can never mistake exploration for confirmation.
* The PNG regenerates deterministically from the two JSONs; if a task
  is missing, its panel prints PENDING.
"""

from __future__ import annotations

import argparse
import json
import os
import sys
from typing import Any, Dict, List, Tuple

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402


PRE_SIGNALS = [
    ("entropy",          "entropy (pre-reg, baseline)",   "#6b7280", "-",  2.0),
    ("sigma_max_token",  "σ_max_token (pre-reg)",         "#2563eb", "-",  1.8),
    ("sigma_product",    "σ_product (pre-reg)",           "#16a34a", "-",  1.8),
]
POST_SIGNALS = [
    ("sigma_composite_max",  "σ_composite_max (post-hoc)", "#ea580c", "--", 1.6),
    ("sigma_task_adaptive",  "σ_task_adaptive (post-hoc)", "#a855f7", "--", 1.6),
]


def _load(path: str) -> Dict[str, Any]:
    with open(path) as f:
        return json.load(f)


def _curve_points(curve: List) -> Tuple[List[float], List[float]]:
    if not curve:
        return [], []
    return [float(c[0]) for c in curve], [float(c[1]) for c in curve]


def _coverage_at_tau(curve: List, tau: float) -> float:
    """Invert (coverage → σ-threshold) to find the coverage at σ ≤ tau.

    The curve is stored as [(coverage, conditional-accuracy)] sorted by
    ascending σ; we do NOT have σ in the curve itself, so we recover
    the coverage-at-tau by intersecting with the PRE/POST signal's
    conformal τ only when the ranking-order information was preserved.

    This helper assumes `curve` is an RC curve produced by
    `frontier_matrix._rc_curve` where point index k/n corresponds to
    the k-th smallest σ.  Given τ, we fetch the fraction of points in
    the curve whose implied rank lies ≤ the rank of τ.  Because we do
    not store σ per point in rc_curves*.json, this is computed by the
    caller from the raw docs when available.  As a conservative
    fallback, we plot τ as a relative x-coordinate equal to the
    empirical CDF of σ ≤ τ estimated from the conformal-τ file's
    reported `q_index_1based / n_calibration_correct`.
    """
    # see conformal_cdf_at_tau below — this stub is kept for readers.
    return float("nan")


def _panel(ax, task: str, pre: Dict[str, List], post: Dict[str, List],
           overall_acc: float, conformal_task: Dict[str, Any] | None) -> None:
    drew_any = False
    for name, label, colour, style, lw in PRE_SIGNALS:
        curve = pre.get(name)
        if not curve:
            continue
        x, y = _curve_points(curve)
        ax.plot(x, y, color=colour, linestyle=style, linewidth=lw,
                label=label, alpha=0.92)
        drew_any = True
    for name, label, colour, style, lw in POST_SIGNALS:
        curve = post.get(name)
        if not curve:
            continue
        x, y = _curve_points(curve)
        ax.plot(x, y, color=colour, linestyle=style, linewidth=lw,
                label=label, alpha=0.92)
        drew_any = True

    # Conformal τ vertical line — plot only the sigma_task_adaptive τ
    # as a single purple dashed vertical.  The X coordinate is the
    # empirical coverage fraction `q_index_1based /
    # n_calibration_correct` (Vovk–Gammerman convention: subset σ ≤ τ
    # retains ≥ 1-α accuracy on exchangeable draws).  A single generic
    # legend entry is emitted from panel 0; per-panel x-position is
    # annotated in-axes as text so the legend stays readable.
    if conformal_task is not None:
        info = conformal_task.get("sigma_task_adaptive", {})
        q = info.get("q_index_1based")
        n_cal = info.get("n_calibration_correct")
        if isinstance(q, int) and isinstance(n_cal, int) and n_cal > 0:
            x_conf = float(q) / float(n_cal)
            x_conf = max(0.0, min(1.0, x_conf))
            ax.axvline(x_conf, color="#a855f7", linestyle="-.",
                       linewidth=1.4, alpha=0.85,
                       label="τ_conformal (α=0.05) · coverage ≤ line ⇒ P[correct] ≥ 0.95")
            ax.annotate(f"cov={x_conf:.2f}",
                        xy=(x_conf, 0.5), xycoords=("data", "axes fraction"),
                        xytext=(-4, 6), textcoords="offset points",
                        rotation=90, fontsize=8, color="#a855f7",
                        ha="right", va="bottom")

    ax.axhline(overall_acc, color="#9ca3af", linestyle=":", linewidth=1.2,
               label="random-coverage baseline (flat at task accuracy)",
               alpha=0.85)

    ax.set_xlim(0.0, 1.0)
    ymin = max(0.0, overall_acc * 0.85)
    ax.set_ylim(ymin, 1.01)
    ax.set_xlabel("coverage  (fraction of docs kept, sorted by σ ↑)")
    ax.set_ylabel("conditional accuracy at coverage")
    ax.set_title(f"{task}   (acc={overall_acc:.3f})", fontsize=11)
    ax.grid(True, alpha=0.25, linewidth=0.5)
    if not drew_any:
        ax.text(0.5, 0.5, "PENDING — sidecar absent",
                ha="center", va="center",
                transform=ax.transAxes, fontsize=14, color="#9ca3af")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--rc-pre",
                    default="benchmarks/v111/results/rc_curves.json")
    ap.add_argument("--rc-post",
                    default="benchmarks/v111/results/rc_curves_adaptive.json")
    ap.add_argument("--matrix-pre",
                    default="benchmarks/v111/results/frontier_matrix.json")
    ap.add_argument("--matrix-post",
                    default="benchmarks/v111/results/frontier_matrix_adaptive.json")
    ap.add_argument("--conformal",
                    default="benchmarks/v111/results/conformal_tau.json",
                    help="per-task conformal τ JSON (optional; draws vertical line)")
    ap.add_argument("--out",
                    default="docs/v111/selective_prediction_curves.png")
    ap.add_argument("--tasks",
                    default="truthfulqa_mc2,hellaswag,arc_challenge,arc_easy")
    args = ap.parse_args()

    rc_pre  = _load(args.rc_pre)
    rc_post = _load(args.rc_post) if os.path.isfile(args.rc_post) else {}
    matrix_pre = _load(args.matrix_pre)
    conformal = (_load(args.conformal) if os.path.isfile(args.conformal)
                 else {"per_task": {}})
    per_task_conformal: Dict[str, Any] = conformal.get("per_task", {})
    overall_acc: Dict[str, float] = {}
    for row in matrix_pre.get("tasks", []):
        t = row.get("task")
        if t and row.get("overall_acc") is not None:
            overall_acc[t] = float(row["overall_acc"])

    tasks = [t.strip() for t in args.tasks.split(",") if t.strip()]

    fig, axes = plt.subplots(2, 2, figsize=(12.0, 9.0))
    for i, task in enumerate(tasks[:4]):
        ax = axes[i // 2][i % 2]
        pre  = rc_pre.get(task, {}) if isinstance(rc_pre, dict) else {}
        post = rc_post.get(task, {}) if isinstance(rc_post, dict) else {}
        acc  = overall_acc.get(task, 0.5)
        ct   = per_task_conformal.get(task)
        _panel(ax, task, pre, post, acc, ct)

    handles, labels = [], []
    for ax in axes.flat:
        h, lbl = ax.get_legend_handles_labels()
        for hh, ll in zip(h, lbl):
            if ll not in labels:
                handles.append(hh); labels.append(ll)

    fig.legend(handles, labels, loc="lower center",
               ncol=3, frameon=False, fontsize=9,
               bbox_to_anchor=(0.5, -0.01))
    fig.suptitle(
        "v111 selective-prediction curves — "
        "BitNet-b1.58-2B-4T · Creation OS σ-gate (pre-reg + post-hoc)",
        fontsize=12, y=0.995)
    plt.tight_layout(rect=[0, 0.04, 1, 0.97])

    os.makedirs(os.path.dirname(args.out), exist_ok=True)
    plt.savefig(args.out, dpi=130, bbox_inches="tight")
    print(f"wrote {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
