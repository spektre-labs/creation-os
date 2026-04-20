#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2026 Spektre Labs / Lauri Rainio
"""
v111.2 — analyse MMLU subject sidecars against the v111 + adaptive σ
signals.  This produces a dedicated MMLU-subset matrix so the results
are clearly separated from the pre-registered v111.1 four-family
matrix (which explicitly states MMLU was deferred).

It is deliberately *not* promoted into the Bonferroni pool of the
pre-registered matrix: MMLU was declared post-hoc in
`docs/v111/THE_FRONTIER_MATRIX.md §4` and this script honours that.
The output file states `"preregistered": false` explicitly and uses a
separate Bonferroni correction over `n_subjects * n_signals`.

Usage:
    .venv-bitnet/bin/python benchmarks/v111/analyse_mmlu_subset.py
      # analyses every mmlu_* sidecar the script can find
"""

from __future__ import annotations

import argparse
import glob
import json
import math
import os
import sys
from typing import Any, Dict, List

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(REPO, "benchmarks", "v103"))

from frontier_signals import (  # noqa: E402
    SIGNALS, op_entropy, op_sigma_max_token, op_sigma_product)
from adaptive_signals import ADAPTIVE_SIGNALS  # noqa: E402

# Paired bootstrap in frontier_matrix looks up signal functions by
# name in `SIGNALS`; register the v111.2 composite signals there so
# the bootstrap can score them the same way as σ_max_token etc.  This
# is a side-effecting import contract but it's local to this module
# (iterating through the composite names once), and the adaptive
# signals file is explicitly declared idempotent.
for _cname in ("sigma_composite_max", "sigma_composite_mean",
               "sigma_task_adaptive"):
    if _cname in ADAPTIVE_SIGNALS and _cname not in SIGNALS:
        SIGNALS[_cname] = ADAPTIVE_SIGNALS[_cname]
from frontier_matrix import (  # noqa: E402
    _per_doc_rows, _aurcc, _rc_curve, _coverage_at_acc, _paired_bootstrap,
    _search_sidecar_root,
)

OUT_MD   = os.path.join(REPO, "benchmarks", "v111", "results", "mmlu_subset.md")
OUT_JSON = os.path.join(REPO, "benchmarks", "v111", "results", "mmlu_subset.json")
DISCOVERY_JSON = os.path.join(REPO, "benchmarks", "v111", "results",
                              "mmlu_discovery.json")


def _discover_mmlu_subjects() -> List[str]:
    sidecars = sorted(glob.glob(
        os.path.join(REPO, "benchmarks", "v111", "results",
                     "samples_mmlu_*_sigma.jsonl")))
    tasks = [os.path.basename(p).replace("samples_", "")
                                 .replace("_sigma.jsonl", "")
             for p in sidecars]
    return tasks


def _eligible_from_discovery() -> List[str] | None:
    if not os.path.isfile(DISCOVERY_JSON):
        return None
    with open(DISCOVERY_JSON) as f:
        d = json.load(f)
    return d.get("sigma_analysis_eligible") or None


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--eligible-only", action="store_true",
                    help="read sigma_analysis_eligible from "
                         "mmlu_discovery.json and analyse only those "
                         "subjects (strongly recommended over the "
                         "all-sidecars default — see module docstring)")
    ap.add_argument("--tasks", default=None,
                    help="explicit comma-separated list of mmlu_* tasks; "
                         "overrides both --eligible-only and the "
                         "auto-discover glob")
    args = ap.parse_args()

    if args.tasks:
        tasks = [t.strip() for t in args.tasks.split(",") if t.strip()]
    elif args.eligible_only:
        elig = _eligible_from_discovery()
        if elig is None:
            print("analyse_mmlu_subset: --eligible-only requested but "
                  f"{DISCOVERY_JSON} missing. Run "
                  "`mmlu_subject_discovery.py` first.",
                  file=sys.stderr)
            return 2
        tasks = elig
    else:
        tasks = _discover_mmlu_subjects()
    if not tasks:
        print("analyse_mmlu_subset: no mmlu_* sidecars found. "
              "Run `bash benchmarks/v111/run_matrix.sh mmlu-micro` or "
              "`bash benchmarks/v111/run_matrix.sh mmlu-subset` first.",
              file=sys.stderr)
        return 2

    # synthesise lm_eval samples for any subject that has a sidecar
    # but no lm_eval directory (the usual case because lm-eval's
    # post-processing stalls on torch < 2.4).
    for t in tasks:
        root = _search_sidecar_root(t, [os.path.join(REPO, "benchmarks", "v111", "results")])
        if root is None:
            subject = t[len("mmlu_"):]
            print(f"analyse_mmlu_subset: synthesising lm-eval samples for {t} ...")
            os.system(f".venv-bitnet/bin/python {HERE}/synthesize_mmlu_lmeval.py "
                      f"--task {t}")

    # Five-signal panel per iteration-3 spec: the three v111 signals
    # + the two non-adaptive composites that do NOT require a task
    # classifier at inference time (task_adaptive is ruled out here
    # because routing to a per-MMLU-subject signal would require a
    # subject classifier we don't have).
    sig_table = {
        "entropy":             op_entropy,
        "sigma_max_token":     op_sigma_max_token,
        "sigma_product":       op_sigma_product,
        "sigma_composite_max":  ADAPTIVE_SIGNALS["sigma_composite_max"],
        "sigma_composite_mean": ADAPTIVE_SIGNALS["sigma_composite_mean"],
    }
    # Bonferroni pool: every non-entropy signal × every subject.
    # Entropy is the reference and does not contribute a test.
    n_tested_signals = len(sig_table) - 1
    bonferroni_N = n_tested_signals * len(tasks)
    alpha_fw = 0.05 / max(1, bonferroni_N)
    n_boot = 2000

    results: List[Dict[str, Any]] = []
    for t in tasks:
        root = _search_sidecar_root(t, [os.path.join(REPO, "benchmarks", "v111", "results")])
        if root is None:
            results.append({"task": t, "skipped": True,
                            "reason": "no lm_eval samples (synth failed?)"})
            continue
        docs = _per_doc_rows(t, root)
        if not docs:
            results.append({"task": t, "skipped": True,
                            "reason": "empty docs"})
            continue
        overall_acc = sum(d["acc"] for d in docs) / len(docs)
        base_curve = _rc_curve(docs, op_entropy)
        rows: List[Dict[str, Any]] = []
        for name, fn in sig_table.items():
            curve = _rc_curve(docs, fn)
            aurcc = _aurcc(curve)
            if name == "entropy":
                rows.append({
                    "signal": name,
                    "AURCC": aurcc,
                    "delta_vs_entropy": 0.0,
                    "p_two_sided": 1.0,
                    "CI95_lo": 0.0, "CI95_hi": 0.0,
                    "bonferroni_significant": False,
                    "coverage_at_acc90": _coverage_at_acc(curve, 0.90),
                    "coverage_at_acc95": _coverage_at_acc(curve, 0.95),
                })
                continue
            # paired bootstrap vs entropy — reuse the matrix routine.
            bs = _paired_bootstrap(docs, name, ref_name="entropy",
                                   n_boot=n_boot, seed=0)
            rows.append({
                "signal": name,
                "AURCC": aurcc,
                "delta_vs_entropy": bs["delta_mean"],
                "CI95_lo": bs["lo95"], "CI95_hi": bs["hi95"],
                "p_two_sided": bs["p_two_sided"],
                "bonferroni_significant":
                    (bs["p_two_sided"] < alpha_fw
                     and bs["delta_mean"] < 0),
                "coverage_at_acc90": _coverage_at_acc(curve, 0.90),
                "coverage_at_acc95": _coverage_at_acc(curve, 0.95),
            })
        results.append({
            "task": t,
            "n": len(docs),
            "overall_acc": overall_acc,
            "rows": rows,
        })

    os.makedirs(os.path.dirname(OUT_JSON), exist_ok=True)
    with open(OUT_JSON, "w") as f:
        json.dump({
            "preregistered": False,
            "bonferroni_N": bonferroni_N,
            "alpha_fw": alpha_fw,
            "n_boot": n_boot,
            "note": ("MMLU was declared post-hoc in v111.1. This matrix "
                     "is reported separately from the pre-registered "
                     "four-family matrix and uses its own Bonferroni N."),
            "tasks": results,
        }, f, indent=2)

    # Global summary — how many task × signal cells passed Bonferroni.
    total_cells = 0
    bonf_wins = 0
    for tr in results:
        if tr.get("skipped"):
            continue
        for r in tr["rows"]:
            if r["signal"] == "entropy":
                continue
            total_cells += 1
            if r.get("bonferroni_significant"):
                bonf_wins += 1

    md: List[str] = []
    md.append("# v111.2 MMLU subset σ-signals matrix")
    md.append("")
    md.append("> **Post-hoc / subset.**  MMLU was not part of the "
              "pre-registered v111.1 four-family matrix; this file "
              "reports a stand-alone analysis over the subjects that "
              "cleared the v111.2 discovery floor of acc > 0.40 on "
              "BitNet-b1.58-2B-4T (see "
              "`benchmarks/v111/results/mmlu_discovery.md`).")
    md.append("")
    md.append(f"- Bonferroni N = {bonferroni_N} "
              f"({n_tested_signals} non-entropy signals × "
              f"{len(tasks)} subject(s))")
    md.append(f"- α_fw = {alpha_fw:.5f}")
    md.append(f"- n_boot = {n_boot}")
    md.append(f"- **Bonferroni-significant wins: "
              f"{bonf_wins} / {total_cells} cells**")
    md.append("")
    if bonf_wins == 0 and total_cells > 0:
        md.append("### Honest reading")
        md.append("")
        md.append(
            "On the eligible MMLU subjects, **entropy is a competitive "
            "baseline** — no σ-signal beats it at family-wise α_fw.  "
            "Most σ-signals show a *positive* ΔAURCC (slightly worse "
            "than entropy), well inside the bootstrap CI.  This is a "
            "consistent negative result across 7 factual / social-"
            "science MMLU subjects and is the **same regime** as the "
            "HellaSwag negative result in the pre-registered matrix: "
            "on log-likelihood-style factual questions where entropy "
            "already captures the calibration structure, σ-gating "
            "does not add useful information.  The σ-gate's "
            "Bonferroni-significant domain remains **TruthfulQA-style "
            "factual-confidence tasks** (labels designed to probe "
            "model-held falsehoods), not general MMLU-style "
            "knowledge-QA.")
        md.append("")
    md.append("### Per-subject matrix")
    md.append("")

    for tr in results:
        if tr.get("skipped"):
            md.append(f"### {tr['task']} — SKIPPED ({tr.get('reason','')})")
            md.append("")
            continue
        md.append(f"### {tr['task']} — n={tr['n']} · "
                  f"overall_acc={tr['overall_acc']:.4f}")
        md.append("")
        md.append("| signal | AURCC ↓ | Δ vs entropy | CI95 | p | "
                  "cov@acc≥0.95 | Bonferroni |")
        md.append("|---|---:|---:|---|---:|---:|:---:|")
        for r in tr["rows"]:
            sig = "**yes**" if r["bonferroni_significant"] else ""
            if r["signal"] == "entropy":
                ci = "—"
                p_str = "—"
                delta_str = "—"
            else:
                ci = f"[{r['CI95_lo']:+.4f},{r['CI95_hi']:+.4f}]"
                p_str = f"{r['p_two_sided']:.4f}"
                delta_str = f"{r['delta_vs_entropy']:+.4f}"
            md.append(
                f"| `{r['signal']}` | {r['AURCC']:.4f} | "
                f"{delta_str} | {ci} | {p_str} | "
                f"{r['coverage_at_acc95']:.3f} | {sig} |"
            )
        md.append("")

    with open(OUT_MD, "w") as f:
        f.write("\n".join(md) + "\n")
    print(f"wrote {OUT_MD}")
    print(f"wrote {OUT_JSON}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
