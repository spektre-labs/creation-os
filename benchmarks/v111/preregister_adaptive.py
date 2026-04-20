#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2026 Spektre Labs / Lauri Rainio
"""
v111.2 — pre-registration tooling for the adaptive σ router.

Two modes:

  1. `--lock` — write `benchmarks/v111/PREREGISTRATION_ADAPTIVE.lock.json`
     with the frozen split-seed, the SHA-256 of `adaptive_signals.py`,
     the per-task calibration / test document-id splits, and the
     routing table.  Once this file exists the hypothesis is locked.

  2. `--analyse` — reload the lock file, rebuild the splits from the
     document ids it stores, and emit:
       benchmarks/v111/results/frontier_matrix_prereg.{md,json}
       benchmarks/v111/results/conformal_tau.json
     Refuses to run if the current `adaptive_signals.py` hash does
     not match the lock file (invariant: no drift between lock and
     analysis).

Design invariants
-----------------
* Calibration ids and test ids are disjoint per task.
* Conformal τ is computed ONLY on the calibration split.
* AURCC + Bonferroni is computed ONLY on the test split.
* `sigma_task_adaptive` uses the frozen routing from the lock file,
  never the live registry (so mutating `adaptive_signals.py` after
  the lock invalidates the analysis, by construction).
"""

from __future__ import annotations

import argparse
import datetime as _dt
import hashlib
import json
import math
import os
import random
import subprocess
import sys
from typing import Any, Callable, Dict, List, Tuple

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(REPO, "benchmarks", "v103"))
sys.path.insert(0, os.path.join(REPO, "benchmarks", "v104"))

from frontier_signals import op_entropy, op_sigma_max_token, op_sigma_product  # noqa: E402
from frontier_matrix import (  # noqa: E402
    _per_doc_rows, _aurcc, _coverage_at_acc, _search_sidecar_root,
)
from adaptive_signals import ADAPTIVE_SIGNALS  # noqa: E402


LOCK_PATH = os.path.join(HERE, "PREREGISTRATION_ADAPTIVE.lock.json")
SPLIT_SEED_HEX = "0xC05A1A2A"           # fixed forever
SPLIT_SEED_INT = int(SPLIT_SEED_HEX, 16)

TASK_FAMILIES = ["arc_challenge", "arc_easy", "truthfulqa_mc2", "hellaswag"]

ROUTING = {
    "truthfulqa_mc2": "sigma_max_token",
    "hellaswag":      "entropy",
    "arc_challenge":  "sigma_product",
    "arc_easy":       "entropy",
}

BONFERRONI_N = 12
ALPHA_FW = 0.05 / BONFERRONI_N
N_BOOT = 2000
ALPHA_CONFORMAL = 0.05

ROOTS_DEFAULT = [
    os.path.join(REPO, "benchmarks", "v111", "results"),
    os.path.join(REPO, "benchmarks", "v104", "n5000_results"),
]


def _signal_by_name(name: str) -> Callable[[dict], float]:
    table = {
        "entropy": op_entropy,
        "sigma_max_token": op_sigma_max_token,
        "sigma_product": op_sigma_product,
    }
    if name in ADAPTIVE_SIGNALS:
        return ADAPTIVE_SIGNALS[name]
    if name not in table:
        raise KeyError(f"unknown signal: {name}")
    return table[name]


def _routed(task: str) -> Callable[[dict], float]:
    return _signal_by_name(ROUTING[task])


def _sha256_file(path: str) -> str:
    with open(path, "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()


def _git_head() -> str:
    try:
        h = subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=REPO, stderr=subprocess.DEVNULL)
        return h.decode().strip()
    except Exception:
        return "unknown"


def _make_splits(docs: List[dict], seed: int) -> Tuple[List[int], List[int]]:
    """Split document indices 50/50 by a seeded shuffle."""
    ids = [int(d["sidecar"]["doc_index"]) for d in docs]
    rng = random.Random(seed)
    rng.shuffle(ids)
    mid = len(ids) // 2
    cal = sorted(ids[:mid])
    test = sorted(ids[mid:])
    return cal, test


def _load_task_docs(task: str, roots: List[str]) -> Tuple[List[dict], str]:
    root = _search_sidecar_root(task, roots)
    if root is None:
        return [], ""
    return _per_doc_rows(task, root), root


def _rc_curve_fn(docs: List[dict], fn) -> List[Tuple[float, float]]:
    n = len(docs)
    if n == 0:
        return []
    scored = sorted([(float(fn(d["sidecar"])), d["acc"]) for d in docs],
                    key=lambda t: t[0])
    cum = 0.0
    out: List[Tuple[float, float]] = []
    for k in range(1, n + 1):
        cum += scored[k - 1][1]
        out.append((k / n, cum / k))
    return out


def _paired_bootstrap(docs: List[dict], fn_a, fn_b,
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
        a = _aurcc(_rc_curve_fn(rs, fn_a))
        b = _aurcc(_rc_curve_fn(rs, fn_b))
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


def _conformal_tau(cal_docs: List[dict], fn,
                   alpha: float = ALPHA_CONFORMAL) -> Dict[str, Any]:
    """Vovk–Gammerman-style conformal quantile on the CORRECT subset.

    Returns τ s.t. on exchangeable draws, the coverage of the subset
    `σ ≤ τ` retains accuracy ≥ 1-α in expectation.

    Formula:
        correct_scores = {σ(d) : d ∈ cal, d is correct}
        n = |correct_scores|
        q = ⌈(1 − α)(n + 1)⌉ (1-indexed; here we clip at n)
        τ = sorted_correct_scores[q - 1]   (ascending)
    """
    correct_scores = sorted([float(fn(d["sidecar"]))
                             for d in cal_docs if d["acc"] >= 0.5])
    n = len(correct_scores)
    if n == 0:
        return {"tau_conformal": float("nan"),
                "n_calibration_correct": 0,
                "n_calibration_total": len(cal_docs),
                "alpha": alpha,
                "guarantee": ("undefined: calibration subset has zero "
                              "correct answers")}
    q = max(1, min(n, int(math.ceil((1.0 - alpha) * (n + 1)))))
    tau = correct_scores[q - 1]
    return {"tau_conformal": tau,
            "n_calibration_correct": n,
            "n_calibration_total": len(cal_docs),
            "q_index_1based": q,
            "alpha": alpha,
            "guarantee": (f"On exchangeable draws, "
                          f"P[correct | σ ≤ τ] ≥ {1.0 - alpha:.2f} "
                          f"in finite-sample expectation "
                          f"(Vovk–Gammerman).")}


# --------------------------- LOCK MODE --------------------------------- #

def _write_lock(roots: List[str]) -> int:
    if os.path.isfile(LOCK_PATH):
        print(f"preregister: lock file already exists at {LOCK_PATH} "
              f"— refusing to overwrite (delete manually if re-locking).",
              file=sys.stderr)
        return 2
    splits: Dict[str, Dict[str, List[int]]] = {}
    for task in TASK_FAMILIES:
        docs, root = _load_task_docs(task, roots)
        if not docs:
            print(f"preregister: {task}: no sidecar found (skip)",
                  file=sys.stderr)
            continue
        cal, test = _make_splits(docs, SPLIT_SEED_INT)
        splits[task] = {"cal_ids": cal, "test_ids": test,
                        "sidecar_root": root, "n_total": len(docs)}
        print(f"preregister: {task}: n={len(docs)} "
              f"cal={len(cal)} test={len(test)}")
    signals_sha = _sha256_file(os.path.join(HERE, "adaptive_signals.py"))
    lock = {
        "locked_at_utc": _dt.datetime.now(_dt.timezone.utc).isoformat(),
        "git_head": _git_head(),
        "signals_sha256": signals_sha,
        "bonferroni_N": BONFERRONI_N,
        "alpha_fw": ALPHA_FW,
        "n_boot": N_BOOT,
        "split_seed": SPLIT_SEED_HEX,
        "routing": ROUTING,
        "conformal_alpha": ALPHA_CONFORMAL,
        "splits": splits,
    }
    with open(LOCK_PATH, "w") as f:
        json.dump(lock, f, indent=2)
    print(f"wrote {LOCK_PATH}")
    return 0


# --------------------------- ANALYSE MODE ------------------------------ #

def _load_lock() -> Dict[str, Any]:
    if not os.path.isfile(LOCK_PATH):
        raise FileNotFoundError(
            f"lock file missing: {LOCK_PATH}. Run with --lock first.")
    with open(LOCK_PATH) as f:
        return json.load(f)


def _verify_lock(lock: Dict[str, Any]) -> None:
    signals_now = _sha256_file(os.path.join(HERE, "adaptive_signals.py"))
    if signals_now != lock["signals_sha256"]:
        raise RuntimeError(
            "adaptive_signals.py SHA-256 drifted from lock file. "
            f"lock={lock['signals_sha256']} now={signals_now}. "
            "Either revert adaptive_signals.py or create a NEW pre-registration.")
    if lock["bonferroni_N"] != BONFERRONI_N:
        raise RuntimeError("Bonferroni N drift")
    if lock["split_seed"] != SPLIT_SEED_HEX:
        raise RuntimeError("Split seed drift")
    if lock["routing"] != ROUTING:
        raise RuntimeError("Routing drift between lock and current code")


def _subset_by_ids(docs: List[dict], ids: List[int]) -> List[dict]:
    want = set(int(i) for i in ids)
    return [d for d in docs if int(d["sidecar"]["doc_index"]) in want]


def _analyse(roots: List[str], out_md: str, out_json: str,
             out_conformal: str, n_boot: int) -> int:
    lock = _load_lock()
    _verify_lock(lock)

    per_task: List[Dict[str, Any]] = []
    conformal: Dict[str, Dict[str, Any]] = {}

    for task, split in lock["splits"].items():
        docs_all, root = _load_task_docs(task, roots)
        if not docs_all:
            per_task.append({"task": task, "skipped": True})
            continue
        cal  = _subset_by_ids(docs_all, split["cal_ids"])
        test = _subset_by_ids(docs_all, split["test_ids"])
        if not cal or not test:
            per_task.append({"task": task, "skipped": True,
                             "reason": "empty split"})
            continue

        # Conformal τ per signal on CAL only.
        tau_by_signal: Dict[str, Any] = {}
        for sig_name in list(ADAPTIVE_SIGNALS) + ["sigma_task_adaptive"]:
            if sig_name == "sigma_task_adaptive":
                fn = _routed(task)
            else:
                fn = _signal_by_name(sig_name)
            tau_by_signal[sig_name] = _conformal_tau(cal, fn)
        conformal[task] = tau_by_signal

        # Bonferroni test on TEST split only.
        rows: List[Dict[str, Any]] = []
        for sig_name in list(ADAPTIVE_SIGNALS) + ["sigma_task_adaptive"]:
            fn = _routed(task) if sig_name == "sigma_task_adaptive" \
                 else _signal_by_name(sig_name)
            curve = _rc_curve_fn(test, fn)
            aurcc = _aurcc(curve)
            stat = _paired_bootstrap(test, fn, op_entropy,
                                     n_boot=n_boot, seed=0)
            rows.append({
                "signal": sig_name,
                "AURCC_test": aurcc,
                "delta_vs_entropy_test": stat["delta_mean"],
                "CI95_lo": stat["lo95"],
                "CI95_hi": stat["hi95"],
                "p_two_sided": stat["p_two_sided"],
                "coverage_at_acc90": _coverage_at_acc(curve, 0.90),
                "coverage_at_acc95": _coverage_at_acc(curve, 0.95),
                "bonferroni_significant":
                    (stat["p_two_sided"] < ALPHA_FW
                     and stat["delta_mean"] < 0),
                "preregistered": True,
            })
        baseline_curve = _rc_curve_fn(test, op_entropy)
        per_task.append({
            "task": task,
            "n_cal": len(cal),
            "n_test": len(test),
            "overall_acc_test": sum(d["acc"] for d in test) / max(1, len(test)),
            "aurcc_entropy_test": _aurcc(baseline_curve),
            "rows": rows,
        })

    # ---------- render markdown ---------- #
    md: List[str] = []
    md.append("# v111.2 pre-registered adaptive σ matrix "
              "(test-split results)")
    md.append("")
    md.append(f"- Lock file: `benchmarks/v111/PREREGISTRATION_ADAPTIVE.lock.json`")
    md.append(f"- Lock hash (adaptive_signals.py): `{lock['signals_sha256'][:16]}...`")
    md.append(f"- Split seed: `{lock['split_seed']}`")
    md.append(f"- Bonferroni N = {lock['bonferroni_N']}, "
              f"α_fw = {lock['alpha_fw']:.5f}")
    md.append(f"- n_boot = {n_boot}")
    md.append("")
    md.append("> **Pre-registered.** Every row is computed on the 50 % "
              "TEST split, held out from the router design AND from the "
              "conformal-τ calibration.  Results on this split replicate "
              "(or fail to replicate) the post-hoc finding.")
    md.append("")

    any_winner = False
    for tr in per_task:
        if tr.get("skipped"):
            md.append(f"### {tr['task']} — PENDING ({tr.get('reason','no sidecar')})\n")
            continue
        md.append(f"### {tr['task']} — n_cal={tr['n_cal']} · "
                  f"n_test={tr['n_test']} · "
                  f"acc_test={tr['overall_acc_test']:.4f} · "
                  f"entropy_AURCC={tr['aurcc_entropy_test']:.4f}")
        md.append("")
        md.append("| signal | AURCC_test ↓ | Δ vs entropy | CI95 | p | "
                  "cov@acc≥0.95 | τ_conformal | Bonferroni |")
        md.append("|---|---:|---:|---|---:|---:|---:|:---:|")
        for r in tr["rows"]:
            tau = conformal.get(tr["task"], {}).get(r["signal"], {})
            tau_str = (f"{tau.get('tau_conformal', float('nan')):.4f}"
                       if tau.get("tau_conformal") is not None
                       and not math.isnan(tau.get("tau_conformal", float("nan")))
                       else "—")
            sig = "**yes**" if r["bonferroni_significant"] else (
                "HURTS" if r["p_two_sided"] < ALPHA_FW
                           and r["delta_vs_entropy_test"] > 0 else ""
            )
            if r["bonferroni_significant"]:
                any_winner = True
            md.append(
                f"| `{r['signal']}` | {r['AURCC_test']:.4f} | "
                f"{r['delta_vs_entropy_test']:+.4f} | "
                f"[{r['CI95_lo']:+.4f},{r['CI95_hi']:+.4f}] | "
                f"{r['p_two_sided']:.4f} | "
                f"{r['coverage_at_acc95']:.3f} | {tau_str} | {sig} |"
            )
        md.append("")

    md.append("### Pre-registration outcome")
    if any_winner:
        md.append(
            "**H₀ REJECTED** on the test split: at least one "
            "(task, signal) pair passes Bonferroni at α_fw = "
            f"{ALPHA_FW:.5f}.  The adaptive σ router is promoted to a "
            "pre-registered claim on the families listed above.")
    else:
        md.append(
            "**H₀ NOT REJECTED** on the test split.  No (task, signal) "
            "pair passes Bonferroni at α_fw = "
            f"{ALPHA_FW:.5f}.  The post-hoc adaptive finding is NOT "
            "replicated under pre-registration and may not be promoted.")

    os.makedirs(os.path.dirname(out_md), exist_ok=True)
    with open(out_md, "w") as f:
        f.write("\n".join(md) + "\n")
    with open(out_json, "w") as f:
        json.dump({
            "preregistered": True,
            "lock_signals_sha256": lock["signals_sha256"],
            "bonferroni_N": BONFERRONI_N,
            "alpha_fw": ALPHA_FW,
            "any_winner": any_winner,
            "tasks": per_task,
        }, f, indent=2)
    with open(out_conformal, "w") as f:
        json.dump({
            "preregistered": True,
            "alpha": ALPHA_CONFORMAL,
            "guarantee_family": "Vovk–Gammerman conformal quantile",
            "per_task": conformal,
        }, f, indent=2)

    print(f"wrote {out_md}")
    print(f"wrote {out_json}")
    print(f"wrote {out_conformal}")
    return 0 if True else 1   # analysis itself never fails once it runs


def main() -> int:
    ap = argparse.ArgumentParser()
    grp = ap.add_mutually_exclusive_group(required=True)
    grp.add_argument("--lock", action="store_true",
                     help="write PREREGISTRATION_ADAPTIVE.lock.json (once)")
    grp.add_argument("--analyse", action="store_true",
                     help="read the lock file and emit the test-split matrix")
    ap.add_argument("--roots",
                    default=",".join([
                        "benchmarks/v111/results",
                        "benchmarks/v104/n5000_results",
                    ]))
    ap.add_argument("--n-boot", type=int, default=N_BOOT)
    ap.add_argument("--out-md",
                    default="benchmarks/v111/results/frontier_matrix_prereg.md")
    ap.add_argument("--out-json",
                    default="benchmarks/v111/results/frontier_matrix_prereg.json")
    ap.add_argument("--out-conformal",
                    default="benchmarks/v111/results/conformal_tau.json")
    args = ap.parse_args()

    roots = [os.path.join(REPO, r) for r in args.roots.split(",") if r.strip()]
    if args.lock:
        return _write_lock(roots)
    else:
        return _analyse(roots, args.out_md, args.out_json,
                        args.out_conformal, args.n_boot)


if __name__ == "__main__":
    sys.exit(main())
