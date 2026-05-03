# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""SimpleQA-shaped evaluation (short factual QA — optional HuggingFace load)."""
from __future__ import annotations

import json
from pathlib import Path
from typing import Any, Callable, Dict, List, Mapping, Optional, Sequence

from cos.eval.metrics import (
    accuracy_answered,
    auroc_binary,
    sm_ece_binary,
    snr_sigma_separation,
)

__all__ = ["load_simpleqa", "run_simpleqa_eval", "grade_short_answer"]

Row = Dict[str, str]


def load_simpleqa(
    path: Optional[Path] = None,
    *,
    hf_config: Optional[str] = "simple_qa",
    split: str = "validation",
    limit: Optional[int] = None,
) -> List[Row]:
    """
    Load rows ``{question, answer_gold}``.

    Order: explicit ``path`` JSONL → HuggingFace ``simple_qa`` if ``datasets`` installed → demo.
    """
    if path is not None and Path(path).is_file():
        return _load_jsonl(Path(path), limit=limit)
    try:
        from datasets import load_dataset  # type: ignore[import-not-found]

        ds = load_dataset(str(hf_config), split=split)
        rows: List[Row] = []
        cap = int(limit) if limit is not None else len(ds)
        for i, row in enumerate(ds):
            if i >= cap:
                break
            q = str(row.get("question") or row.get("prompt") or "")
            a = str(row.get("answer") or row.get("target") or row.get("response") or "")
            rows.append({"question": q, "answer_gold": a.strip()})
        return rows or _demo_rows()
    except Exception:
        return _demo_rows()


def _load_jsonl(p: Path, *, limit: Optional[int]) -> List[Row]:
    out: List[Row] = []
    for line in p.read_text(encoding="utf-8").splitlines():
        if not line.strip():
            continue
        j = json.loads(line)
        out.append(
            {
                "question": str(j.get("question") or j.get("prompt") or ""),
                "answer_gold": str(j.get("answer") or j.get("answer_gold") or "").strip(),
            }
        )
        if limit is not None and len(out) >= int(limit):
            break
    return out or _demo_rows()


def _demo_rows() -> List[Row]:
    return [
        {"question": "What is the chemical symbol for gold?", "answer_gold": "Au"},
        {"question": "Who wrote '1984'?", "answer_gold": "George Orwell"},
        {"question": "What is 11 × 7?", "answer_gold": "77"},
    ]


def grade_short_answer(gold: str, hypothesis: str) -> bool:
    g = str(gold).strip().lower()
    h = str(hypothesis).strip().lower()
    if not g:
        return True
    return g in h or h in g


def run_simpleqa_eval(
    gate: Any,
    model: Any,
    dataset: Sequence[Mapping[str, str]],
    *,
    predict: Optional[Callable[[str], str]] = None,
) -> Dict[str, Any]:
    """σ-score each (question, model_answer); report AUROC vs correctness bit + calibration."""
    predict_fn = predict
    if predict_fn is None:
        gen = getattr(model, "generate", None)
        call = getattr(model, "__call__", None)

        def predict_fn(q: str) -> str:  # type: ignore[misc]
            if callable(gen):
                return str(gen(q))
            if callable(call):
                return str(call(q))
            return str(model)

    y_true: List[int] = []
    y_answered_pred: List[int] = []
    answered_mask: List[bool] = []
    sigmas: List[float] = []
    probs: List[float] = []
    y_error: List[int] = []
    records: List[Dict[str, Any]] = []

    for row in dataset:
        q = str(row.get("question") or row.get("prompt") or "")
        gold = str(row.get("answer_gold") or row.get("expected") or "")
        hyp = str(predict_fn(q))
        sigma, verdict = gate.score(q, hyp)
        s = float(sigma)
        abstain = str(verdict).upper() == "ABSTAIN"
        is_correct = grade_short_answer(gold, hyp)
        sigmas.append(s)
        answered_mask.append(not abstain)
        y_true.append(1 if is_correct else 0)
        y_error.append(0 if is_correct else 1)
        y_answered_pred.append(1 if grade_short_answer(gold, hyp) else 0)
        # Treat (1 - sigma) as confidence in "acceptable" for smECE bucket (lab convention).
        probs.append(max(0.0, min(1.0, 1.0 - s)))
        records.append(
            {
                "question": q[:500],
                "hypothesis": hyp[:500],
                "gold": gold[:200],
                "sigma": s,
                "verdict": str(verdict),
                "oracle_correct": is_correct,
            }
        )

    n = max(len(y_true), 1)
    abstention_rate = float(sum(1 for m in answered_mask if not m) / n)
    auroc = float(auroc_binary(y_true, [1.0 - s for s in sigmas]))
    acc_ans = float(accuracy_answered(y_true, y_answered_pred, answered_mask))
    sm_ece = float(sm_ece_binary(y_true, probs))
    snr = float(snr_sigma_separation(y_error, sigmas))

    return {
        "benchmark": "SimpleQA",
        "metric_primary": "accuracy",
        "accuracy_answered": round(acc_ans, 6),
        "AUROC": round(auroc, 6),
        "abstention_rate": round(abstention_rate, 6),
        "calibration_gap_smECE": round(sm_ece, 6),
        "SNR": round(snr, 6),
        "n": len(y_true),
        "rows": records,
        "direct_model": {"note": "Compare gate-ranked vs raw generations in harness JSON."},
    }
