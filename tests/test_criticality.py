# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.criticality`."""
from __future__ import annotations

from cos.criticality import Criticality


def test_phase_frozen_critical_chaotic() -> None:
    c = Criticality(critical_sigma=0.35)
    assert c.phase(0.1) == "FROZEN"
    assert c.phase(0.3) == "CRITICAL"
    assert c.phase(0.6) == "CHAOTIC"


def test_nudge_explore_simplify_maintain() -> None:
    c = Criticality(critical_sigma=0.35)
    assert c.nudge_toward_critical(0.05)["action"] == "EXPLORE"
    assert c.nudge_toward_critical(0.6)["action"] == "SIMPLIFY"
    assert c.nudge_toward_critical(0.3)["action"] == "MAINTAIN"


def test_is_critical_window() -> None:
    c = Criticality(critical_sigma=0.35)
    # Mean 0.35, variance ~0.0025 (within (0.001, 0.05))
    base = [0.25, 0.45, 0.30, 0.40, 0.33, 0.37, 0.35, 0.35, 0.35, 0.35]
    for s in base:
        c.record(s)
    assert c.is_critical(window=10)


def test_power_law_check_insufficient() -> None:
    c = Criticality()
    for s in range(10):
        c.record(float(s) / 20.0)
    r = c.power_law_check()
    assert r["power_law"] is False


def test_status_and_distance() -> None:
    c = Criticality(critical_sigma=0.35)
    assert c.status()["phase"] == "UNKNOWN"
    c.record(0.4)
    st = c.status()
    assert st["current_sigma"] == 0.4
    assert st["phase"] == "CRITICAL"
    assert c.distance_from_criticality(0.35) == 0.0
