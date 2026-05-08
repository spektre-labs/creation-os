# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.energy` (budget-aware allocation lab)."""
from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.energy import EnergyAware  # noqa: E402


class _FixedDifficulty:
    def __init__(self, sigma: float) -> None:
        self._sigma = float(sigma)

    def score(self, _prompt: str, _response: str):
        return self._sigma, "ACCEPT"


def test_available_full_budget() -> None:
    e = EnergyAware(budget_usd=100.0, gate=_FixedDifficulty(0.2))
    a = e.available()
    assert a["cognitive_mode"] == "DEEP"
    assert abs(float(a["budget_remaining_pct"]) - 100.0) < 0.01


def test_allocate_deep_mode() -> None:
    e = EnergyAware(budget_usd=50.0, gate=_FixedDifficulty(0.25))
    out = e.allocate("solve this multi-step proof")
    assert out["tokens"] == 1024
    assert out["cascade_depth"] == 5


def test_allocate_survival_mode() -> None:
    e = EnergyAware(budget_usd=10.0, gate=_FixedDifficulty(0.1))
    e.record_cost(9.75)
    out = e.allocate("any prompt")
    assert out["tokens"] == 64
    assert out["model"] == "local_3b"
    assert "survival" in out["reason"].lower()


def test_should_migrate_low_budget() -> None:
    e = EnergyAware(budget_usd=10.0)
    e.record_cost(9.76)
    m = e.should_migrate()
    assert m["migrate"] is True
    assert "low" in m["reason"].lower()


def test_shutdown_triggers_dream() -> None:
    e = EnergyAware(budget_usd=100.0)
    e.record_cost(96.0)
    s = e.shutdown_recommendation()
    assert s["action"] == "SLEEP"
    assert "dream" in s["reason"].lower()
