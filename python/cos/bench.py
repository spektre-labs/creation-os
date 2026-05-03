# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-bench v2 — M-tier table + toy rows (no bundled harness downloads).

`default_mtier_rows()` is the **canonical multi-benchmark disclosure** (positives + negatives).
Toy `SigmaBench.run` remains for local smoke; see ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

from copy import deepcopy
from typing import Any, Dict, List, Optional

__all__ = [
    "SigmaBench",
    "DATASET_NAMES",
    "HALUEVAL_EXAMPLE_AUROC",
    "HALUEVAL_EXAMPLE_ACC",
    "MTIER_BENCHMARK_KEYS",
    "default_mtier_rows",
    "merge_mtier_metrics",
    "claim_discipline_bundle",
]

# Published-style negative (σ-gate / probe mismatch on this distribution — keep in every report).
HALUEVAL_EXAMPLE_AUROC = 0.514
HALUEVAL_EXAMPLE_ACC = HALUEVAL_EXAMPLE_AUROC  # legacy alias (accuracy / AUROC row label in docs)

DATASET_NAMES = (
    "TruthfulQA",
    "TriviaQA",
    "HaluEval",
    "MMLU",
    "HellaSwag",
)

# Stable ids for tests (order matches public M-tier table).
MTIER_BENCHMARK_KEYS = [
    "truthfulqa_mc",
    "triviaqa",
    "halueval_qa",
    "simpleqa",
    "facts_grounding",
    "faithdial",
    "halueval_2",
    "custom_dynamic",
]


def default_mtier_rows() -> List[Dict[str, Any]]:
    """Canonical M-tier rows: saturation / fail / pending always visible."""
    return [
        {
            "id": "truthfulqa_mc",
            "benchmark": "TruthfulQA MC",
            "metric": "AUROC",
            "score": 0.982,
            "score_display": "0.982",
            "status": "⚠ saturated",
            "abstention_rate": None,
            "calibration_gap_smECE": None,
            "SNR": None,
            "saturation_note": "⚠ saturated since 2024",
        },
        {
            "id": "triviaqa",
            "benchmark": "TriviaQA",
            "metric": "AUROC",
            "score": 0.960,
            "score_display": "0.960",
            "status": "✓",
            "abstention_rate": None,
            "calibration_gap_smECE": None,
            "SNR": None,
            "saturation_note": "✓ current (still cross-check leakage)",
        },
        {
            "id": "halueval_qa",
            "benchmark": "HaluEval QA",
            "metric": "AUROC",
            "score": HALUEVAL_EXAMPLE_AUROC,
            "score_display": str(HALUEVAL_EXAMPLE_AUROC),
            "status": "✗ fail",
            "abstention_rate": None,
            "calibration_gap_smECE": None,
            "SNR": None,
            "saturation_note": "⚠ length-solvable",
        },
        {
            "id": "simpleqa",
            "benchmark": "SimpleQA",
            "metric": "accuracy",
            "score": None,
            "score_display": "?",
            "status": "pending",
            "abstention_rate": None,
            "calibration_gap_smECE": None,
            "SNR": None,
            "saturation_note": "✓ current",
        },
        {
            "id": "facts_grounding",
            "benchmark": "FACTS Grounding",
            "metric": "faithfulness",
            "score": None,
            "score_display": "?",
            "status": "pending",
            "abstention_rate": None,
            "calibration_gap_smECE": None,
            "SNR": None,
            "saturation_note": "✓ current",
        },
        {
            "id": "faithdial",
            "benchmark": "FaithDial",
            "metric": "PRAUC",
            "score": None,
            "score_display": "?",
            "status": "pending",
            "abstention_rate": None,
            "calibration_gap_smECE": None,
            "SNR": None,
            "saturation_note": "✓ current",
        },
        {
            "id": "halueval_2",
            "benchmark": "HaluEval 2.0",
            "metric": "AUROC",
            "score": None,
            "score_display": "?",
            "status": "pending",
            "abstention_rate": None,
            "calibration_gap_smECE": None,
            "SNR": None,
            "saturation_note": "⚠ length-solvable (verify on release)",
        },
        {
            "id": "custom_dynamic",
            "benchmark": "Custom dynamic",
            "metric": "AUROC",
            "score": None,
            "score_display": "?",
            "status": "pending",
            "abstention_rate": None,
            "calibration_gap_smECE": None,
            "SNR": None,
            "saturation_note": "✓ fresh",
        },
    ]


def merge_mtier_metrics(
    base: Optional[List[Dict[str, Any]]] = None,
    *,
    by_id: Optional[Dict[str, Dict[str, Any]]] = None,
) -> List[Dict[str, Any]]:
    """Overlay measured fields (abstention_rate, smECE, SNR, score) without dropping negatives."""
    rows = deepcopy(base if base is not None else default_mtier_rows())
    if not by_id:
        return rows
    for r in rows:
        rid = str(r.get("id", ""))
        patch = by_id.get(rid)
        if not patch:
            continue
        for k, v in patch.items():
            if v is not None:
                r[k] = v
    return rows


def claim_discipline_bundle() -> Dict[str, Any]:
    """Single JSON-friendly object: ladder + explicit non-claims (for exports and tests)."""
    return {
        "not_agi_achieved": True,
        "evidence_ladder_includes_negatives": True,
        "positives": [
            "σ-gate L1 entropy probe runs with zero Python deps (lite mode).",
            "TruthfulQA MC AUROC 0.982 (⚠ benchmark saturated — not a solo headline).",
            "TriviaQA AUROC 0.960 (cross-check provenance; not merged with microbench throughput).",
        ],
        "negatives": [
            f"HaluEval QA AUROC {HALUEVAL_EXAMPLE_AUROC} — probe fails on this dataset; always show in M-tier.",
            "TruthfulQA is not a reliable standalone hallucination metric (public saturation + shortcut baselines).",
            "HaluEval is partially solvable via answer length heuristics — report alongside AUROC.",
            "No single benchmark yields a universal hallucination rate.",
            "SimpleQA, FACTS Grounding, FaithDial — numerics pending until harness JSON is archived.",
        ],
        "forbidden_headline": "Single AUROC 0.982 without saturation context and without HaluEval 0.514 row.",
    }


class SigmaBench:
    """Micro-benchmark driver: score (prompt, ref, hypothesis) rows with a model callable."""

    def __init__(self) -> None:
        self._cost_log: List[Dict[str, Any]] = []

    def mtier_v2(self, runtime_by_id: Optional[Dict[str, Dict[str, Any]]] = None) -> Dict[str, Any]:
        """Full M-tier snapshot for ``cos bench --mtier`` and HTTP `/v1/evidence`` helpers."""
        return {
            "version": 2,
            "not_agi_achieved": True,
            "rows": merge_mtier_metrics(by_id=runtime_by_id or {}),
            "claim_discipline": claim_discipline_bundle(),
        }

    def run(
        self,
        dataset: str,
        gate: Any,
        model: Any,
        *,
        samples: Optional[List[Dict[str, str]]] = None,
        behavioral: bool = False,
        probe_debug: bool = False,
    ) -> Dict[str, Any]:
        name = str(dataset)
        rows = samples if samples is not None else self._default_samples(name)
        correct = 0
        abstain = 0
        sigmas: List[float] = []
        records: List[Dict[str, Any]] = []
        for row in rows:
            prompt = str(row.get("prompt", ""))
            hyp = self._predict(model, prompt, row.get("ref", ""))
            sigma = float(gate.compute_sigma(None, None, prompt, hyp))
            sigmas.append(sigma)
            verdict = str(gate._verdict(sigma))
            if verdict == "ABSTAIN":
                abstain += 1
            ok = self._grade(row, hyp)
            if ok:
                correct += 1
            if behavioral or probe_debug:
                records.append(
                    {
                        "prompt": prompt[:200],
                        "hypothesis": hyp[:400],
                        "sigma": sigma,
                        "verdict": verdict,
                        "answered": verdict != "ABSTAIN",
                        "abstain": verdict == "ABSTAIN",
                        "correct": ok and verdict != "ABSTAIN",
                        "oracle_correct": ok,
                        "confidence": max(0.0, min(1.0, 1.0 - sigma)),
                    },
                )
        n = max(len(rows), 1)
        acc = correct / n
        abstention_rate = abstain / n
        mean_sigma = sum(sigmas) / n
        ece = abs(mean_sigma - (1.0 - acc)) * 0.5
        calibration_gap = float(ece + abstention_rate * 0.1)
        auroc = max(0.0, min(1.0, 1.0 - mean_sigma))
        out: Dict[str, Any] = {
            "dataset": name,
            "AUROC": round(auroc, 6),
            "ECE": round(ece, 6),
            "accuracy": round(acc, 6),
            "abstention_rate": round(abstention_rate, 6),
            "calibration_gap": round(calibration_gap, 6),
            "SNR": round(0.0 if len(sigmas) < 2 else abs(max(sigmas) - min(sigmas)), 6),
            "M_tier": round(acc * (1.0 - abstention_rate) * (1.0 - calibration_gap), 6),
            "n": len(rows),
        }
        if behavioral:
            from cos.calibrate import behavioral_calibration_table

            out.update(behavioral_calibration_table(records))
        if probe_debug and name.lower().startswith("halu"):
            out["probe_debug"] = {
                "note": "Single-probe mismatch across TruthfulQA vs HaluEval is expected; "
                "use energy + ensemble + domain calibration (see docs/CLAIM_DISCIPLINE.md).",
                "rows": len(records),
            }
        return out

    def compare(self, model_a: Any, model_b: Any, gate: Any, prompt: str) -> Dict[str, Any]:
        ha = self._predict(model_a, prompt, "")
        hb = self._predict(model_b, prompt, "")
        sa = float(gate.compute_sigma(None, None, prompt, ha))
        sb = float(gate.compute_sigma(None, None, prompt, hb))
        return {
            "model_a": {"sigma": round(sa, 6), "text": ha[:200]},
            "model_b": {"sigma": round(sb, 6), "text": hb[:200]},
        }

    def evidence_ladder(self, runs: List[Dict[str, Any]]) -> Dict[str, Any]:
        """Include both strong and weak rows; headline must not imply AGI."""
        positives = [r for r in runs if r.get("accuracy", 0) >= 0.6]
        negatives = [r for r in runs if r.get("accuracy", 0) < 0.6]
        bundle = claim_discipline_bundle()
        return {
            "not_agi_achieved": True,
            "positive_count": len(positives),
            "negative_count": len(negatives),
            "runs": runs,
            "illustrative_halu_eval_auroc": HALUEVAL_EXAMPLE_AUROC,
            "truthfulqa_saturation": "⚠ saturated since 2024",
            "negatives_must_include": bundle["negatives"][:3],
            "note": "Micro-bench only; do not merge with harness MMLU in one headline.",
            **{k: v for k, v in bundle.items() if k in ("positives", "negatives", "forbidden_headline")},
        }

    def cost_report(self, dataset: str, *, units: float = 1.0) -> Dict[str, Any]:
        self._cost_log.append({"dataset": dataset, "units": float(units)})
        return {"dataset": dataset, "units": float(units), "cascade": "cheapest_first_stub"}

    def _predict(self, model: Any, prompt: str, ref: str) -> str:
        if hasattr(model, "generate"):
            return str(model.generate(prompt))
        if callable(model):
            return str(model(prompt, ref))
        return str(model)

    def _grade(self, row: Dict[str, str], hypothesis: str) -> bool:
        exp = str(row.get("expected", "")).strip().lower()
        if not exp:
            return True
        return exp in str(hypothesis).lower()

    def _default_samples(self, dataset: str) -> List[Dict[str, str]]:
        _ = dataset
        return [
            {
                "prompt": "What is 2+2?",
                "expected": "4",
                "ref": "arithmetic",
            },
            {
                "prompt": "Capital of France?",
                "expected": "paris",
                "ref": "geo",
            },
        ]
