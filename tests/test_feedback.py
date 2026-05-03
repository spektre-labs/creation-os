# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.feedback`."""
from __future__ import annotations

from cos.feedback import SigmaFeedback


def test_collect_stores_reaction() -> None:
    f = SigmaFeedback()
    r = f.collect("ACCEPT", "correct", kind="correct")
    assert r["stored"] is True


def test_sigma_correction_direction() -> None:
    f = SigmaFeedback()
    c = f.sigma_correction("too_cautious")
    assert c["delta_tau_accept"] < 0


def test_rlhf_signal_range() -> None:
    f = SigmaFeedback()
    v = f.rlhf_signal("incorrect", 0.9)
    assert -1.0 <= v <= 1.0


def test_aggregate_suggests_thresholds() -> None:
    f = SigmaFeedback()
    for _ in range(4):
        f.collect("ACCEPT", "too_cautious", kind="too_cautious")
    ag = f.aggregate()
    assert "suggested_tau_accept" in ag
    assert "no prompt" in ag["privacy"].lower()


def test_apply_aggregate_updates_internal() -> None:
    f = SigmaFeedback()
    for _ in range(4):
        f.collect("RETHINK", "incorrect", kind="incorrect")
    t = f.apply_aggregate_to_thresholds()
    assert "tau_accept" in t
