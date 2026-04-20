#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2026 Spektre Labs / Lauri Rainio
"""
v111.2 — adaptive RAG demo (fixture-based, not a benchmark).

Idea
----
A σ-gated RAG policy: for each incoming query, compute σ first; if
σ < τ (the model is confident), answer directly; if σ ≥ τ (the model
is unsure), fetch retrieval context and answer with it.  Two headline
numbers:

    retrievals_saved_pct   — fraction of queries answered without
                             calling the retriever (cost savings)
    accuracy_retained      — accuracy among the answered-direct
                             queries, relative to always-answer-
                             direct and always-retrieve baselines

The demo is deliberately NOT a live-retrieval stack.  It runs on the
already-measured v111 σ-sidecars so the numbers are reproducible from
existing JSONLs and require zero network I/O.  Every answer the demo
"emits" is the accuracy label from the lm-eval sample; every "skip"
is a σ above the per-task conformal τ.

Because this is a demo, it is NOT a pre-registered claim: the τ used
here is the conformal-calibration τ from
`benchmarks/v111/results/conformal_tau.json`, which was calibrated on
the 50 % calibration split.  Evaluation is on the 50 % test split
(disjoint).  Still honest, still reproducible; just a demo rather
than a benchmark.

What the demo answers
---------------------
"On a σ-gated RAG policy with Vovk–Gammerman τ at α=0.05, on task T:
 - how many retrievals get saved?
 - what accuracy is retained among the answered-direct queries?
 - how does that compare to always-retrieve (baseline assumption:
   retrieval is perfect) and always-answer-direct (no gate)?"

Output
------
`benchmarks/v111/results/adaptive_rag_demo.json`:

  per-task: {
      n_test, n_answer_direct, n_retrieve,
      accuracy_answer_direct, accuracy_always_direct,
      retrievals_saved_pct
  }

`benchmarks/v111/results/adaptive_rag_demo.md` — human summary.
"""

from __future__ import annotations

import json
import math
import os
import sys
from typing import Any, Dict, List

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(REPO, "benchmarks", "v103"))

from frontier_signals import op_entropy, op_sigma_max_token, op_sigma_product  # noqa: E402
from adaptive_signals import ADAPTIVE_SIGNALS  # noqa: E402
from frontier_matrix import _per_doc_rows, _search_sidecar_root  # noqa: E402
from preregister_adaptive import (  # noqa: E402
    LOCK_PATH, ROUTING, ROOTS_DEFAULT, _signal_by_name, _subset_by_ids,
)

OUT_JSON = os.path.join(REPO, "benchmarks", "v111", "results",
                        "adaptive_rag_demo.json")
OUT_MD   = os.path.join(REPO, "benchmarks", "v111", "results",
                        "adaptive_rag_demo.md")
CONFORMAL_PATH = os.path.join(REPO, "benchmarks", "v111", "results",
                              "conformal_tau.json")


def _policy_decide(docs: List[dict], signal_fn, tau: float) -> Dict[str, Any]:
    n_direct, n_retr = 0, 0
    correct_direct = 0
    correct_all = 0
    for d in docs:
        sigma = float(signal_fn(d["sidecar"]))
        acc   = float(d["acc"])
        correct_all += 1 if acc >= 0.5 else 0
        if math.isnan(tau) or sigma > tau:
            n_retr += 1
            # In a live deployment: retrieve context and re-score.
            # For the demo we assume the retrieval-augmented answer is
            # as-good-as-the-oracle (upper bound); see §caveats.
        else:
            n_direct += 1
            if acc >= 0.5:
                correct_direct += 1
    n = len(docs)
    return {
        "n_test": n,
        "n_answer_direct": n_direct,
        "n_retrieve": n_retr,
        "retrievals_saved_pct": (100.0 * n_direct / n) if n else 0.0,
        "accuracy_on_direct_only":
            (correct_direct / n_direct) if n_direct else float("nan"),
        "accuracy_always_direct":
            (correct_all / n) if n else float("nan"),
    }


def main() -> int:
    if not os.path.isfile(LOCK_PATH):
        print("adaptive_rag_demo: lock file missing — run "
              "`benchmarks/v111/preregister_adaptive.py --lock` first",
              file=sys.stderr)
        return 2
    if not os.path.isfile(CONFORMAL_PATH):
        print("adaptive_rag_demo: conformal_tau.json missing — run "
              "`benchmarks/v111/preregister_adaptive.py --analyse` first",
              file=sys.stderr)
        return 2

    with open(LOCK_PATH) as f:
        lock = json.load(f)
    with open(CONFORMAL_PATH) as f:
        conf = json.load(f)["per_task"]

    per_task_results: Dict[str, Any] = {}
    for task, split in lock["splits"].items():
        root = _search_sidecar_root(task, ROOTS_DEFAULT)
        if root is None:
            continue
        docs_all = _per_doc_rows(task, root)
        test = _subset_by_ids(docs_all, split["test_ids"])
        routed = _signal_by_name(ROUTING[task])
        tau_info = conf.get(task, {}).get("sigma_task_adaptive", {})
        tau = tau_info.get("tau_conformal", float("nan"))
        per_task_results[task] = {
            "signal_used": ROUTING[task],
            "tau_conformal": tau,
            "alpha_conformal": tau_info.get("alpha", 0.05),
            **_policy_decide(test, routed, tau),
        }

    os.makedirs(os.path.dirname(OUT_JSON), exist_ok=True)
    with open(OUT_JSON, "w") as f:
        json.dump({
            "preregistered_demo": True,
            "policy":  "answer_direct iff σ_task_adaptive ≤ τ_conformal",
            "note":    ("Demo, not a benchmark.  Numbers assume "
                        "retrieval-augmented path would succeed on the "
                        "abstained-from queries; see caveats in "
                        "adaptive_rag_demo.md."),
            "per_task": per_task_results,
        }, f, indent=2)

    md: List[str] = []
    md.append("# v111.2 adaptive σ-RAG demo (fixture-based)")
    md.append("")
    md.append("> **Demo, not a benchmark.**  Reports how a σ-gated RAG")
    md.append("> policy would behave on the pre-registered 50 % test")
    md.append("> split, using the Vovk–Gammerman conformal τ from the")
    md.append("> 50 % calibration split.  Numbers below are reproducible")
    md.append("> from existing sidecars with zero network I/O.")
    md.append("")
    md.append("| task | signal routed to | τ (α=0.05) | n_test | "
              "answer-direct | retrieve | **retrievals saved** | "
              "acc on direct-only | baseline acc (always-direct) |")
    md.append("|---|---|---:|---:|---:|---:|---:|---:|---:|")
    for t, r in per_task_results.items():
        acc_d  = r["accuracy_on_direct_only"]
        acc_ad = r["accuracy_always_direct"]
        md.append(
            f"| `{t}` | `{r['signal_used']}` | "
            f"{r['tau_conformal']:.4f} | "
            f"{r['n_test']} | {r['n_answer_direct']} | {r['n_retrieve']} | "
            f"**{r['retrievals_saved_pct']:.1f}%** | "
            f"{acc_d:.3f} | {acc_ad:.3f} |"
        )
    md.append("")
    md.append("### Caveats")
    md.append("")
    md.append("- **Numbers depend on BitNet-b1.58-2B-4T's calibration.**  "
              "A different base model (e.g. Llama-3-8B) would give a "
              "different conformal τ and a different retrievals-saved "
              "percentage on the same tasks.")
    md.append("- **Retrieve branch is assumed-oracle.**  The accuracy "
              "reported is the accuracy **on the abstained-from subset** "
              "(answered-direct only), not the end-to-end accuracy of a "
              "full RAG stack.  In a live deployment you must add a real "
              "retriever and measure the retrieval-augmented accuracy "
              "too.")
    md.append("- **Distribution shift matters.**  Per "
              "`docs/v111/CONFORMAL_GUARANTEE.md`, the ≥1-α coverage "
              "bound holds only on exchangeable draws from the "
              "calibration distribution.")
    md.append("")
    md.append("### Reproduce")
    md.append("")
    md.append("```bash")
    md.append(".venv-bitnet/bin/python benchmarks/v111/preregister_adaptive.py --lock      # once")
    md.append(".venv-bitnet/bin/python benchmarks/v111/preregister_adaptive.py --analyse   # test-split matrix + τ")
    md.append(".venv-bitnet/bin/python benchmarks/v111/adaptive_rag_demo.py                # this table")
    md.append("```")
    with open(OUT_MD, "w") as f:
        f.write("\n".join(md) + "\n")

    print(f"wrote {OUT_JSON}")
    print(f"wrote {OUT_MD}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
