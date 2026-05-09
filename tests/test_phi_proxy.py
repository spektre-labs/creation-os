# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.theory.phi_proxy import PhiProxy  # noqa: E402


def test_phi_zero_when_stable() -> None:
    p = PhiProxy()
    for _ in range(8):
        p.record(0.5)
    assert p.phi() == 0.0


def test_phi_high_when_changing() -> None:
    p = PhiProxy()
    for v in (0.1, 0.9, 0.1, 0.9, 0.1):
        p.record(v)
    assert p.phi() >= 0.7


def test_learning_phase_exploration() -> None:
    p = PhiProxy()
    for v in (0.9, 0.2, 0.9, 0.2, 0.85, 0.25):
        p.record(v)
    assert p.learning_phase() == "exploration"


def test_learning_phase_mastery() -> None:
    p = PhiProxy()
    for _ in range(12):
        p.record(0.1)
    assert p.learning_phase() == "mastery"


def test_integration_whole_better() -> None:
    p = PhiProxy()
    p.record(0.15)
    out = p.integration([0.85, 0.9])
    assert out["integrated"] is True
    assert out["phi_proxy"] > 0.05


def test_integration_not_detected() -> None:
    p = PhiProxy()
    p.record(0.6)
    out = p.integration([0.25, 0.3])
    assert out["integrated"] is False
    assert out["interpretation"] == "no integration detected"
