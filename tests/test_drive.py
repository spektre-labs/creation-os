# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from cos.drive import SigmaDrive


def test_curiosity_when_sigma_drops() -> None:
    d = SigmaDrive()
    for s in (1.0, 0.85, 0.7, 0.55):
        d.record(s)
    assert d.emotion()["state"] == "curiosity"


def test_anxiety_when_sigma_rises() -> None:
    d = SigmaDrive()
    for s in (0.45, 0.6, 0.75, 0.9):
        d.record(s)
    assert d.emotion()["state"] == "anxiety"


def test_satisfaction_at_low_sigma() -> None:
    d = SigmaDrive()
    d.record(0.09)
    d.record(0.08)
    assert d.emotion()["state"] == "satisfaction"


def test_frustration_when_stuck() -> None:
    d = SigmaDrive()
    d.record(0.72)
    d.record(0.721)
    assert d.emotion()["state"] == "frustration"


def test_should_continue_curiosity() -> None:
    d = SigmaDrive()
    for s in (1.0, 0.9, 0.8, 0.7):
        d.record(s)
    assert d.should_continue() is True


def test_should_stop_frustration() -> None:
    d = SigmaDrive()
    d.record(0.68)
    d.record(0.681)
    assert d.should_continue() is False
