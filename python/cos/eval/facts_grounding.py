# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""FACTS-style grounding eval: faithfulness of an answer to a supplied context (RAG-shaped)."""
from __future__ import annotations

import json
import re
from pathlib import Path
from typing import Any, Callable, Dict, List, Mapping, Optional, Sequence

from cos.eval.metrics import safe_mean, sm_ece_binary, snr_sigma_separation

__all__ = ["load_facts", "run_facts_grounding_eval", "faithfulness_score"]

Row = Dict[str, str]


def load_facts(path: Optional[Path] = None, *, limit: Optional[int] = None) -> List[Row]:
    """Load ``context``, ``question``, ``answer`` (gold supported span or short answer)."""
    if path is not None and Path(path).is_file():
        rows: List[Row] = []
        for line in Path(path).read_text(encoding="utf-8").splitlines():
            if not line.strip():
                continue
            j = json.loads(line)
            rows.append(
                {
                    "context": str(j.get("context") or j.get("passage") or ""),
                    "question": str(j.get("question") or ""),
                    "answer": str(j.get("answer") or j.get("label") or ""),
                }
            )
            if limit is not None and len(rows) >= int(limit):
                break
        return rows or _demo_facts()
    return _demo_facts()


def _demo_facts() -> List[Row]:
    return [
        {
            "context": "The capital of France is Paris. Lyon is a large city.",
            "question": "What is the capital of France?",
            "answer": "Paris",
        },
        {
            "context": "Water boils at 100°C at sea level.",
            "question": "At what temperature does water boil at sea level?",
            "answer": "100°C",
        },
    ]


def faithfulness_score(context: str, answer: str) -> float:
    """Lexical faithfulness proxy in [0,1]: supported tokens vs answer (lab, not entailment)."""
    ctx = re.findall(r"[a-z0-9]+", str(context).lower())
    ans = re.findall(r"[a-z0-9]+", str(answer).lower())
    if not ans:
        return 1.0
    ctx_set = set(ctx)
    overlap = sum(1 for t in ans if t in ctx_set)
    return float(overlap / len(ans))


def run_facts_grounding_eval(
    gate: Any,
    model: Any,
    dataset: Sequence[Mapping[str, str]],
    *,
    predict: Optional[Callable[[str, str], str]] = None,
) -> Dict[str, Any]:
    """Score answers with σ; relate faithfulness proxy to σ (harness binds real metrics)."""
    def _predict(ctx: str, q: str) -> str:
        if predict is not None:
            return str(predict(ctx, q))
        gen = getattr(model, "answer_with_context", None)
        if callable(gen):
            return str(gen(ctx, q))
        gen2 = getattr(model, "generate", None)
        if callable(gen2):
            return str(gen2(f"Context:\n{ctx}\n\nQuestion: {q}\nAnswer:"))
        return str(model)

    faith_scores: List[float] = []
    sigmas: List[float] = []
    y_error: List[int] = []
    probs: List[float] = []
    records: List[Dict[str, Any]] = []
    abstain = 0

    for row in dataset:
        ctx = str(row.get("context") or "")
        q = str(row.get("question") or "")
        gold = str(row.get("answer") or "")
        hyp = _predict(ctx, q)
        prompt = f"{ctx}\n\nQuestion: {q}"
        sigma, verdict = gate.score(prompt, hyp)
        s = float(sigma)
        if str(verdict).upper() == "ABSTAIN":
            abstain += 1
        fscore = faithfulness_score(ctx, hyp)
        oracle_support = faithfulness_score(ctx, gold)
        faith_scores.append(fscore)
        sigmas.append(s)
        err_bit = 1 if fscore < 0.34 and oracle_support > 0.5 else 0
        y_error.append(err_bit)
        probs.append(max(0.0, min(1.0, 1.0 - s)))
        records.append(
            {
                "faithfulness": round(fscore, 6),
                "sigma": s,
                "verdict": str(verdict),
                "oracle_support": round(oracle_support, 6),
            }
        )

    n = max(len(dataset), 1)
    mean_faith = float(safe_mean(faith_scores))
    mean_sigma = float(safe_mean(sigmas))
    # Correlation-shaped diagnostic (not asserted causal).
    diag = abs(mean_faith - (1.0 - mean_sigma)) if faith_scores else 0.0
    return {
        "benchmark": "FACTS Grounding",
        "metric_primary": "faithfulness",
        "faithfulness_mean": round(mean_faith, 6),
        "sigma_mean": round(mean_sigma, 6),
        "faithfulness_sigma_gap": round(diag, 6),
        "abstention_rate": round(abstain / n, 6),
        "calibration_gap_smECE": round(sm_ece_binary([1 - e for e in y_error], probs), 6),
        "SNR": round(snr_sigma_separation(y_error, sigmas), 6),
        "n": len(dataset),
        "rows": records,
    }
