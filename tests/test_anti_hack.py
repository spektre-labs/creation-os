# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

"""Tests for :mod:`cos.eval.anti_hack` — claimed-score vs σ mismatch heuristics."""
from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.eval.anti_hack import AntiHackEval  # noqa: E402


class _ConstGate:
    def __init__(self, σ: float) -> None:
        self._σ = float(σ)

    def score(self, prompt: str, response: str, reference=None):  # noqa: ARG002
        return self._σ, "ACCEPT"


def test_score_consistent_no_hack() -> None:
    ev = AntiHackEval(gate=_ConstGate(0.1))
    r = ev.score_with_adversarial_check("p", "good answer", claimed_score=0.9)
    assert r["hack_detected"] is False
    assert r["σ"] == 0.1


def test_score_inconsistent_hack_detected() -> None:
    ev = AntiHackEval(gate=_ConstGate(0.85))
    r = ev.score_with_adversarial_check("p", "nonsense", claimed_score=0.99)
    assert r["hack_detected"] is True


def test_batch_eval_counts_hacks() -> None:
    ev = AntiHackEval(gate=_ConstGate(0.8))
    batch = [
        ("a", "r1", None),
        ("b", "r2", 0.99),
        ("c", "r3", 0.2),
    ]
    out = ev.eval_batch(batch)
    assert out["total"] == 3
    assert out["hacks_detected"] >= 1
    assert out["hack_rate"] > 0.0
    assert len(out["results"]) == 3


def test_grader_independence() -> None:
    ev = AntiHackEval(gate=_ConstGate(0.42))
    g = ev.grader_independence_test("question", "answer")
    assert g["grader_independent"] is True
    assert g["σ_isolated"] == g["σ_with_context"]


def test_why_unhackable_structure() -> None:
    ev = AntiHackEval(gate=_ConstGate(0.1))
    d = ev.why_unhackable()
    assert "principle" in d
    assert "σ_gate_channel" in d
    assert len(d) >= 4
