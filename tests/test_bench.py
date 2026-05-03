# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import pytest

from cos.bench import DATASET_NAMES, HALUEVAL_EXAMPLE_ACC, SigmaBench
from cos.sigma_gate import SigmaGate


@pytest.mark.bench
def test_run_returns_metrics() -> None:
    b = SigmaBench()
    g = SigmaGate()
    r = b.run("MMLU", g, lambda p, ref="": "4" if "2+2" in str(p) else "paris")
    for k in (
        "AUROC",
        "ECE",
        "accuracy",
        "abstention_rate",
        "calibration_gap",
        "SNR",
        "M_tier",
        "dataset",
    ):
        assert k in r


def test_compare_side_by_side() -> None:
    b = SigmaBench()
    g = SigmaGate()

    def a(p: str, r: str = "") -> str:
        return "x" * 50

    def short(_p: str, _r: str = "") -> str:
        return "4"

    c = b.compare(a, short, g, "What is 2+2?")
    assert "model_a" in c and "model_b" in c
    assert "sigma" in c["model_a"]


def test_evidence_ladder_neg_pos() -> None:
    b = SigmaBench()
    g = SigmaGate()
    good = b.run(
        "TriviaQA",
        g,
        lambda p, r="": "4" if "2+2" in str(p) else "paris",
    )
    bad = b.run(
        "HaluEval",
        g,
        lambda _p, _r="": "nope",
        samples=[{"prompt": "p", "expected": "yes", "ref": "r"}],
    )
    lad = b.evidence_ladder([good, bad])
    assert lad["not_agi_achieved"] is True
    assert lad["negative_count"] >= 1
    assert lad["positive_count"] >= 1


def test_cost_report_cascade_stub() -> None:
    b = SigmaBench()
    r = b.cost_report("TruthfulQA", units=2.0)
    assert r["cascade"] == "cheapest_first_stub"
    assert r["units"] == 2.0


def test_dataset_names_tuple() -> None:
    assert "MMLU" in DATASET_NAMES
    assert "HellaSwag" in DATASET_NAMES


def test_halueval_example_constant_documented() -> None:
    assert HALUEVAL_EXAMPLE_ACC == 0.514
