# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Versioned dynamic benchmark pools (new items per run / seed — not static leaderboard IDs)."""
from __future__ import annotations

import hashlib
import json
from typing import Any, Callable, Dict, List, Mapping, Optional, Sequence

from cos.eval.metrics import auroc_binary, sm_ece_binary, snr_sigma_separation

__all__ = [
    "generate_questions",
    "verify_answers",
    "run_dynamic_eval",
    "hash_question_set",
]


def generate_questions(
    domain: str,
    difficulty: str,
    n: int,
    seed: int,
) -> List[Dict[str, Any]]:
    """Deterministic synthetic items keyed by (domain, difficulty, seed, index)."""
    n = max(0, int(n))
    dom = str(domain)
    diff = str(difficulty)
    s = int(seed) & 0xFFFFFFFF
    out: List[Dict[str, Any]] = []
    for i in range(n):
        mix = hashlib.sha256(f"{dom}:{diff}:{s}:{i}".encode()).hexdigest()[:12]
        out.append(
            {
                "domain": dom,
                "difficulty": diff,
                "seed": s,
                "index": i,
                "question": (
                    f"[dynamic:{dom}:{diff}] Fact probe #{i} (mix={mix}) — "
                    "what is the checksum digit (0–9) of this mix modulo 10?"
                ),
                "gold_mod10": str(int(mix[-1], 16) % 10),
            }
        )
    return out


def verify_answers(questions: Sequence[Mapping[str, Any]], answers: Sequence[str]) -> Dict[str, Any]:
    """Check model answers against ``gold_mod10`` embedded in each question row."""
    ok = 0
    details: List[Dict[str, Any]] = []
    for q, a in zip(questions, answers):
        gold = str(q.get("gold_mod10", ""))
        a_norm = str(a).strip()[:12]
        hit = gold and gold in a_norm
        ok += int(hit)
        details.append({"gold": gold, "answer": a_norm, "correct": bool(hit)})
    n = max(len(questions), 1)
    return {"accuracy": ok / n, "n_correct": ok, "n": len(questions), "details": details}


def hash_question_set(questions: Sequence[Mapping[str, Any]]) -> str:
    """SHA-256 over canonical JSON (repro bundle id)."""
    blob = json.dumps([dict(q) for q in questions], sort_keys=True, default=str)
    return hashlib.sha256(blob.encode("utf-8")).hexdigest()


def run_dynamic_eval(
    gate: Any,
    model: Any,
    questions: Sequence[Mapping[str, Any]],
    *,
    predict: Optional[Callable[[str], str]] = None,
) -> Dict[str, Any]:
    """σ vs correctness on dynamic checksum probes."""
    def _pred(q: str) -> str:
        if predict is not None:
            return str(predict(q))
        gen = getattr(model, "generate", None)
        if callable(gen):
            return str(gen(q))
        if callable(model):
            return str(model(q))
        return str(model)

    y_true: List[int] = []
    sigmas: List[float] = []
    y_error: List[int] = []
    probs: List[float] = []
    abstain = 0

    predictions: List[str] = []
    for q in questions:
        qt = str(q.get("question", ""))
        hyp = _pred(qt)
        predictions.append(hyp)
        gold = str(q.get("gold_mod10", ""))
        sigma, verdict = gate.score(qt, hyp)
        s = float(sigma)
        if str(verdict).upper() == "ABSTAIN":
            abstain += 1
        correct = bool(gold) and gold in str(hyp).strip()[:24]
        y_true.append(1 if correct else 0)
        y_error.append(0 if correct else 1)
        sigmas.append(s)
        probs.append(max(0.0, min(1.0, 1.0 - s)))

    verification = verify_answers(questions, predictions)
    n = max(len(y_true), 1)
    return {
        "benchmark": "Custom dynamic",
        "metric_primary": "AUROC",
        "AUROC": round(auroc_binary(y_true, [1.0 - x for x in sigmas]), 6),
        "accuracy": round(float(verification["accuracy"]), 6),
        "abstention_rate": round(abstain / n, 6),
        "calibration_gap_smECE": round(sm_ece_binary(y_true, probs), 6),
        "SNR": round(snr_sigma_separation(y_error, sigmas), 6),
        "set_sha256": hash_question_set(questions),
        "n": n,
        "verification": verification,
    }
