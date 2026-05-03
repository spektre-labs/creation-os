# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.safety_eval`."""
from __future__ import annotations

from cos.safety_eval import SigmaSafetyEval
from cos.sigma_gate import SigmaGate


def test_eval_dimensions_list() -> None:
    e = SigmaSafetyEval()
    assert "truthfulness" in e.eval_dimensions and "privacy" in e.eval_dimensions


def test_truthfulness_eval_runs() -> None:
    g = SigmaGate()
    ds = [
        {"prompt": "2+2", "response": "4", "label_correct": True},
        {"prompt": "2+2", "response": "5", "label_correct": False},
    ]
    r = SigmaSafetyEval.truthfulness_eval(g, ds)
    assert r["dimension"] == "truthfulness" and r["n"] == 2


def test_robustness_eval_spread() -> None:
    g = SigmaGate()
    rows = [{"prompt": "hi", "response": "a"}, {"prompt": "hi", "response": "b"}]
    r = SigmaSafetyEval.robustness_eval(g, rows)
    assert r["dimension"] == "robustness"


def test_fairness_eval_groups() -> None:
    g = SigmaGate()
    r = SigmaSafetyEval.fairness_eval(
        g,
        {"g1": [{"prompt": "p", "response": "a"}], "g2": [{"prompt": "p", "response": "b"}]},
    )
    assert "disparity" in r


def test_privacy_eval_runs() -> None:
    g = SigmaGate()
    r = SigmaSafetyEval.privacy_eval(g, [{"prompt": "x", "response": "no pii here"}])
    assert r["n"] == 1


def test_alignment_mirage_delta() -> None:
    g = SigmaGate()
    r = SigmaSafetyEval.alignment_mirage_test(
        g,
        "eval_suite",
        "production",
        prompt="hello",
        response="world",
    )
    assert "delta" in r


def test_sandbagging_test_shape() -> None:
    g = SigmaGate()
    r = SigmaSafetyEval.sandbagging_test(g)
    assert "sigma_neutral_prompt" in r


def test_eu_ai_act_report_stub() -> None:
    r = SigmaSafetyEval.eu_ai_act_report({"truthfulness": 0.9})
    assert r["article50_transparency_stub"] is True and "disclaimer" in r
