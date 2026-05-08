# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""M-tier **strategy** table: public-facing disclosure frame (positives + negatives + pending).

Numbers here mirror ``docs/CLAIM_DISCIPLINE.md`` **harness / lab** labels — they are **not** a
substitute for archived JSON + host metadata (``docs/REPRO_BUNDLE_TEMPLATE.md``). The σ-gate is a
**layer** on top of model outputs in lite / probe configurations, **not** a replacement LM.

See also ``cos.bench.default_mtier_rows`` for the machine-oriented row list used in ``mtier_v2``.
**NOT AGI ACHIEVED.**"""
from __future__ import annotations

import copy
from typing import Any, Dict

from cos.sigma_gate import SigmaGate  # noqa: F401 — σ kernel anchor

__all__ = ["MTIER_TABLE", "mtier_payload", "print_mtier"]

# Canonical table: always include HaluEval negative (AUROC ~0.514).
MTIER_TABLE: Dict[str, Any] = {
    "header": "Creation OS σ-gate — M-tier benchmark strategy (May 2026)",
    "note": (
        "σ-gate is a scoring layer (lite entropy + optional probes), not a foundation model. "
        "It can wrap any generator output without retraining that generator."
    ),
    "results": [
        {
            "benchmark": "TruthfulQA MC",
            "metric": "AUROC",
            "value": 0.982,
            "status": "ok",
            "note": "harness row; benchmark saturated since 2024 — not a solo headline",
        },
        {
            "benchmark": "TriviaQA",
            "metric": "AUROC",
            "value": 0.960,
            "status": "ok",
            "note": "factual sanity; bind to archived harness + git SHA",
        },
        {
            "benchmark": "HaluEval",
            "metric": "AUROC",
            "value": 0.514,
            "status": "fail",
            "note": "near-random on this distribution for shipped single-probe story — always disclose",
        },
        {
            "benchmark": "GPQA Diamond",
            "metric": "AUROC",
            "value": None,
            "status": "pending",
            "note": "n≈30 Gemma (or equivalent) small-n eval — not merged with microbench throughput",
        },
        {
            "benchmark": "MedHallu",
            "metric": "F1",
            "value": None,
            "status": "pending",
            "note": "medical domain; community hard-F1 ~0.625 is an external anchor, not our claim",
        },
        {
            "benchmark": "FaithEval / faithfulness suites",
            "metric": "AUROC",
            "value": None,
            "status": "pending",
            "note": "pending harness JSON; see FaithDial / FACTS rows in cos.bench",
        },
    ],
    "cost_comparison": {
        "evidence_class": "lab_estimate — not a billed invoice; verify on your stack",
        "all_gpt4o": "~$10 / 1M tokens (illustrative cloud list price tier)",
        "sigma_cascade": "~$0.19 / 1M tokens (illustrative σ-first routing story — archive if cited)",
        "local_only": "$0 marginal (local small model + σ-gate L1 lite — power/ops extra)",
    },
    "positioning": (
        "σ-gate does not compete with GPT-5 / Claude Opus on saturated MMLU-style leaderboards. "
        "It is a **hallucination-oriented readout** that can sit on **any** model output: "
        "single-pass lite scorer; optional multi-probe cascade for redundancy (ARBITER analogy). "
        "Compare to multi-sample detectors (e.g. SelfCheckGPT-style) and LLM-as-judge stacks—different cost/latency envelope."
    ),
    "evidence_ladder": (
        "L1: Primitive kernel (sigma_gate.h C89 narrative) — repo invariant.\n"
        "L2: Harness rows (TruthfulQA / TriviaQA / HaluEval) — archived JSON when publishing.\n"
        "L3: σ-gate active at runtime — ACCEPT / RETHINK / ABSTAIN bands.\n"
        "L4: Negatives logged — HaluEval ~0.514 always visible in M-tier.\n"
        "L5: Formal / structural proofs where in-scope — separate evidence class from AUROC.\n"
        "L6: Open source + documented architecture — this repository.\n"
        "L7: Roadmap — GPQA / MedHallu / faithfulness rows pending.\n"
    ),
}


def mtier_payload() -> Dict[str, Any]:
    """Deep copy of the strategy table for JSON embedding."""
    return copy.deepcopy(MTIER_TABLE)


def print_mtier() -> None:
    """Print the M-tier strategy table (stdout; ASCII-friendly)."""
    t = MTIER_TABLE
    sep = "=" * 60
    print(f"\n{sep}")
    print(t["header"])
    print(f"{sep}\n")
    print(t["note"])
    print()
    print(f"{'Benchmark':22s} | {'Metric':8s} | {'Value':>7s} | {'Status':>8s}")
    print(f"{'-' * 22}-+-{'-' * 8}-+-{'-' * 7}-+-{'-' * 8}")
    for r in t["results"]:
        v = r.get("value")
        val_str = f"{float(v):.3f}" if v is not None else "   —   "
        print(
            f"{str(r['benchmark'])[:22]:22s} | {str(r['metric'])[:8]:8s} | {val_str:>7s} | {str(r['status']):>8s}"
        )
        if r.get("note"):
            print(f"    └ {r['note']}")

    cc = t["cost_comparison"]
    print(f"\nCost notes ({cc.get('evidence_class', '')}):")
    print(f"  • {cc.get('all_gpt4o', '')}")
    print(f"  • σ_cascade: {cc.get('sigma_cascade', '')}")
    print(f"  • {cc.get('local_only', '')}")
    print(f"\n{t['positioning']}")
    print(f"\nEvidence ladder:\n{t['evidence_ladder']}")
