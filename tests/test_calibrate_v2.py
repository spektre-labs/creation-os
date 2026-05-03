# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from cos.calibrate import (
    adaptive_threshold_snr,
    behavioral_calibration_table,
    false_negative_abstain_rate,
    log_snr_accuracy_hallucination,
    smECE,
    true_positive_answered_accuracy,
)


def test_smECE_small_grid() -> None:
    c = [0.1, 0.4, 0.7, 0.9]
    e = [1.0, 0.0, 1.0, 0.0]
    s = smECE(c, e, n_grid=12)
    assert s >= 0.0


def test_log_snr() -> None:
    v = log_snr_accuracy_hallucination(0.8, 0.2)
    assert v > 0


def test_true_positive_answered() -> None:
    recs = [
        {"answered": True, "correct": True, "confidence": 0.9},
        {"answered": True, "correct": False, "confidence": 0.8},
    ]
    assert true_positive_answered_accuracy(recs) == 0.5


def test_false_negative_abstain() -> None:
    recs = [
        {"abstain": True, "oracle_correct": True},
        {"abstain": True, "oracle_correct": False},
    ]
    assert false_negative_abstain_rate(recs) == 0.5


def test_behavioral_calibration_table_keys() -> None:
    recs = [
        {
            "answered": True,
            "abstain": False,
            "correct": True,
            "sigma": 0.2,
            "confidence": 0.85,
            "oracle_correct": True,
        },
    ]
    tab = behavioral_calibration_table(recs)
    for k in ("smECE", "SNR_log", "M_tier_behavioral", "accuracy_answered"):
        assert k in tab


def test_adaptive_threshold_snr_runs() -> None:
    base = [
        {"sigma": 0.2, "correct": True, "oracle_correct": True},
        {"sigma": 0.9, "correct": False, "oracle_correct": False},
    ]
    out = adaptive_threshold_snr(base, taus=(0.25, 0.5, 0.75))
    assert "tau_accept_proxy" in out
