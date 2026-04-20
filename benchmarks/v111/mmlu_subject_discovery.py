#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2026 Spektre Labs / Lauri Rainio
"""
v111.2 — MMLU subject discovery (accuracy-only sweep).

Why this exists
---------------
σ-gated selective prediction cannot improve a model that is at random
accuracy.  On a 4-choice task, random = 0.25; BitNet b1.58-2B-4T on
`mmlu_abstract_algebra` measured 0.33 in v111.2, which is random + noise.
There is no selective-prediction signal to discover there: the underlying
model does not know the answer, so σ carries no usable uncertainty
information.

This script is the floor check.  It runs BitNet on a *curated* MMLU
candidate list (no σ logging, just `_per_doc_rows`-compatible accuracy)
and reports which subjects clear a prior-defined accuracy floor.  Only
those pass forward to the σ-analysis in `analyse_mmlu_subset.py`.

Why not run all 57 subjects
---------------------------
A full 57-subject sweep at the default MMLU split (~14.0 k questions ×
4 candidates × ≈0.8 s/ll on M3) is ≈5 h of wall time.  That is out of
scope for a single iteration.  Instead, we seed a small candidate list
from two *documented external* sources — not fabricated — and sweep
those at n=100 each (≈30 min total):

  1. The BitNet b1.58-2B-4T paper (Ma et al., 2504.12285) reports
     MMLU 0-shot ≈ 53.2 % average.  Subjects well above that average
     across comparable 2B-class models tend to be factual /
     social-science / health; subjects well below tend to be pure
     math / formal logic.
  2. Standard MMLU difficulty literature (Hendrycks et al. 2009.03300,
     Table 3): hardest subjects are the `college_*` STEM family and
     `abstract_algebra`, `formal_logic`, `high_school_mathematics`;
     easiest are `marketing`, `sociology`, `high_school_psychology`,
     `global_facts`, `us_foreign_policy`, `human_sexuality`.

The candidate list below is a transparent prior, not a measurement.
The script *measures* accuracy for each candidate and decides the
σ-analysis set at runtime.

Output
------
`benchmarks/v111/results/mmlu_discovery.json`:

    {
      "timestamp": "...",
      "candidate_list_source": "bitnet-paper + hendrycks-2020",
      "limit_per_subject": 100,
      "subjects": [
        {"task": "mmlu_marketing", "n": 100, "acc": 0.67, "passed_40_floor": true},
        {"task": "mmlu_sociology", "n": 100, "acc": 0.63, "passed_40_floor": true},
        ...
      ],
      "sigma_analysis_eligible": ["mmlu_marketing", "mmlu_sociology", ...]
    }

`benchmarks/v111/results/mmlu_discovery.md` — human summary.
"""

from __future__ import annotations

import argparse
import glob
import json
import os
import subprocess
import sys
from datetime import datetime, timezone

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(REPO, "benchmarks", "v103"))

OUT_JSON = os.path.join(REPO, "benchmarks", "v111", "results",
                        "mmlu_discovery.json")
OUT_MD = os.path.join(REPO, "benchmarks", "v111", "results",
                      "mmlu_discovery.md")

# ---------------------------------------------------------------- prior -- #

# Candidate list seeded from two external sources (see module docstring).
# Each tuple is (task_slug, rationale) for transparency.  The rationale
# is recorded in the output JSON so reviewers can audit the prior.
CANDIDATE_SUBJECTS: list[tuple[str, str]] = [
    # --- likely-easy (2B-class factual / social-science / health) ---
    ("mmlu_marketing",                    "easy: Hendrycks 2020 Table 3 easy-cluster"),
    ("mmlu_sociology",                    "easy: Hendrycks 2020 Table 3 easy-cluster"),
    ("mmlu_high_school_psychology",       "easy: factual + large n"),
    ("mmlu_nutrition",                    "easy: health facts, 2B paper avg-above"),
    ("mmlu_us_foreign_policy",            "easy: Hendrycks 2020 easy-cluster"),
    ("mmlu_global_facts",                 "easy: Hendrycks 2020 easy-cluster"),
    # --- medium (sanity checks, 2B paper near average) ---
    ("mmlu_high_school_government_and_politics",
                                          "medium: factual, widely used in leaderboards"),
    ("mmlu_human_sexuality",              "medium: 131-q factual set, 2B avg-above"),
    # --- hard controls (floor-check: should NOT pass) ---
    ("mmlu_high_school_mathematics",      "hard-control: should fall below 40% floor"),
    ("mmlu_abstract_algebra",             "hard-control: already measured 0.33 in v111.2"),
]

# Acceptance floor.  See module docstring: random = 0.25 on 4-choice,
# 0.40 is the prior-registered cutoff for "selective prediction has
# meaningful room to operate".
ACCURACY_FLOOR = 0.40

# -------------------------------------------------------------- runner -- #


def _already_cached_sidecar(task: str) -> str | None:
    p = os.path.join(REPO, "benchmarks", "v111", "results",
                     f"samples_{task}_sigma.jsonl")
    return p if os.path.isfile(p) else None


def _run_one_subject(task: str, limit: int, py: str,
                     timeout_s: int) -> int:
    """Runs one MMLU subject under a wall-clock timeout.

    lm-eval's post-processing step stalls indefinitely on hosts with
    torch < 2.4 (it prints "Disabling PyTorch …" after writing the
    sidecar and then never returns), so we set a hard wall-clock
    timeout that's long enough for BitNet inference of `limit`
    questions × 4 candidates (~0.25 s/ll on M3 ≈ 100 q × 4 × 0.25 s =
    100 s) plus padding.  If the subprocess is still alive after that
    timeout, we kill it; the sidecar is already on disk and
    `synthesize_mmlu_lmeval.py` recovers the accuracy from it.

    Returns:
      0 if the sidecar is populated enough to score (regardless of
        whether lm-eval itself exited cleanly),
      non-zero if we cannot get enough rows (e.g. BitNet crashed).
    """
    existing = _already_cached_sidecar(task)
    if existing and os.path.getsize(existing) > 0:
        with open(existing) as f:
            n_rows = sum(1 for _ in f)
        if n_rows >= limit * 4:
            return 0

    cmd = [
        "bash", os.path.join(HERE, "run_matrix.sh"),
        f"mmlu-discovery-single:{task}:{limit}",
    ]
    try:
        subprocess.run(cmd, timeout=timeout_s)
    except subprocess.TimeoutExpired:
        # Expected: lm-eval post-processing stall after inference.
        # Kill the entire process group so lm-eval subprocesses go too.
        subprocess.run(["pkill", "-f", f"task_tag={task}"],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        subprocess.run(["pkill", "-f", f"tasks {task}"],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    existing = _already_cached_sidecar(task)
    if not existing or os.path.getsize(existing) == 0:
        return 1
    with open(existing) as f:
        n_rows = sum(1 for _ in f)
    # Require at least half the target rows so we know BitNet actually
    # ran (a handful of rows could just be startup).  On partial rows
    # we still score what's there, but flag the subject.
    if n_rows < (limit * 4) // 2:
        return 2
    return 0


def _measure_accuracy(task: str) -> tuple[int, float] | None:
    """Synthesises the lm-eval samples for `task` from the sidecar +
    HF gold labels (same trick as analyse_mmlu_subset.py) and returns
    (n, acc)."""
    sidecar = _already_cached_sidecar(task)
    if not sidecar:
        return None

    py = os.path.join(REPO, ".venv-bitnet", "bin", "python")
    subprocess.run(
        [py, os.path.join(HERE, "synthesize_mmlu_lmeval.py"),
         "--task", task],
        check=False, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    synth_dir = os.path.join(
        REPO, "benchmarks", "v111", "results", "lm_eval", task,
        "synth_from_sidecar")
    files = sorted(glob.glob(
        os.path.join(synth_dir, f"samples_{task}_*.jsonl")))
    if not files:
        return None
    latest = files[-1]
    n = 0
    correct = 0.0
    with open(latest) as f:
        for ln in f:
            row = json.loads(ln)
            n += 1
            correct += float(row.get("acc", 0.0))
    if n == 0:
        return None
    return n, correct / n


# ---------------------------------------------------------------- main -- #


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--limit", type=int, default=100,
                    help="max questions per subject (default 100)")
    ap.add_argument("--skip-run", action="store_true",
                    help="only re-score cached sidecars, don't re-run BitNet")
    ap.add_argument("--subjects", default=None,
                    help="comma-separated override for the candidate list")
    ap.add_argument("--timeout", type=int, default=180,
                    help="per-subject wall-clock timeout in seconds "
                         "(default 180; set higher for slow hosts)")
    args = ap.parse_args()

    if args.subjects:
        candidates = [(s.strip(), "user-supplied") for s in args.subjects.split(",")
                      if s.strip()]
    else:
        candidates = CANDIDATE_SUBJECTS

    py = os.path.join(REPO, ".venv-bitnet", "bin", "python")
    results: list[dict] = []
    for task, rationale in candidates:
        if not args.skip_run:
            rc = _run_one_subject(task, args.limit, py, args.timeout)
            if rc != 0:
                results.append({
                    "task": task, "rationale": rationale,
                    "n": 0, "acc": None,
                    "passed_40_floor": False,
                    "error": f"run_matrix.sh rc={rc}",
                })
                continue
        m = _measure_accuracy(task)
        if m is None:
            results.append({
                "task": task, "rationale": rationale,
                "n": 0, "acc": None,
                "passed_40_floor": False,
                "error": "accuracy not measurable (no sidecar / no synth)",
            })
            continue
        n, acc = m
        results.append({
            "task": task,
            "rationale": rationale,
            "n": n,
            "acc": acc,
            "passed_40_floor": bool(acc > ACCURACY_FLOOR),
        })

    eligible = [r["task"] for r in results if r.get("passed_40_floor")]

    out = {
        "timestamp": datetime.now(timezone.utc).isoformat(),
        "candidate_list_source": "bitnet-paper-2504.12285 + hendrycks-2009.03300",
        "limit_per_subject": args.limit,
        "accuracy_floor": ACCURACY_FLOOR,
        "random_accuracy": 0.25,
        "subjects": results,
        "sigma_analysis_eligible": eligible,
    }

    os.makedirs(os.path.dirname(OUT_JSON), exist_ok=True)
    with open(OUT_JSON, "w") as f:
        json.dump(out, f, indent=2)

    md: list[str] = []
    md.append("# v111.2 MMLU subject discovery (accuracy floor)")
    md.append("")
    md.append(f"- Candidate source: `{out['candidate_list_source']}`")
    md.append(f"- Limit per subject: **{args.limit} questions**")
    md.append(f"- Accuracy floor for σ-analysis: **> {ACCURACY_FLOOR:.2f}** "
              f"(random on 4-choice = 0.25)")
    md.append("")
    md.append("> Floor is prior-registered in the script, not tuned on measured data.")
    md.append("> σ-gated selective prediction requires non-trivial base accuracy; "
              "a model at random has no uncertainty structure to exploit.")
    md.append("")
    md.append("| task | n | acc | vs random | passed floor | rationale |")
    md.append("|---|---:|---:|---:|:---:|---|")
    for r in sorted(results, key=lambda x: -(x.get("acc") or 0.0)):
        acc = r.get("acc")
        acc_s = f"{acc:.3f}" if acc is not None else "—"
        vs = f"+{(acc - 0.25):+.3f}" if acc is not None else "—"
        ok = "**yes**" if r.get("passed_40_floor") else ""
        rat = r.get("rationale", "")
        err = r.get("error")
        if err:
            rat = f"{rat} · ERROR: {err}"
        md.append(f"| `{r['task']}` | {r.get('n', 0)} | {acc_s} | "
                  f"{vs} | {ok} | {rat} |")
    md.append("")
    md.append(f"**σ-analysis eligible (acc > {ACCURACY_FLOOR:.2f}):** "
              f"{len(eligible)} / {len(results)} subjects")
    md.append("")
    for t in eligible:
        md.append(f"- `{t}`")
    md.append("")
    md.append("### What happens next")
    md.append("")
    md.append(
        "Only the eligible subjects are passed into "
        "`benchmarks/v111/analyse_mmlu_subset.py` for the full "
        "σ-AURCC + Bonferroni matrix.  Subjects below the floor are "
        "honestly reported as *excluded because the base model is "
        "near-random*, not as σ-gate failures.")
    md.append("")
    md.append("### Reproduce")
    md.append("")
    md.append("```bash")
    md.append(".venv-bitnet/bin/python benchmarks/v111/mmlu_subject_discovery.py \\")
    md.append("    --limit 100")
    md.append(".venv-bitnet/bin/python benchmarks/v111/analyse_mmlu_subset.py \\")
    md.append("    --eligible-only   # reads sigma_analysis_eligible from discovery")
    md.append("```")

    with open(OUT_MD, "w") as f:
        f.write("\n".join(md) + "\n")

    print(f"wrote {OUT_JSON}")
    print(f"wrote {OUT_MD}")
    print(f"sigma_analysis_eligible = {eligible}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
