# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for ``cos.budget``."""
from __future__ import annotations

from cos.budget import SigmaBudget
from cos.sigma_gate import SigmaGate


def test_allocate_tier_from_mapping() -> None:
    b = SigmaBudget(SigmaGate())
    r = b.allocate({"prompt": "2+2", "response": "4"})
    assert "tier" in r and r["budget_eur"] > 0
    assert b.cost_per_verdict("ACCEPT") < b.cost_per_verdict("RETHINK")


def test_token_budget_division() -> None:
    r = SigmaBudget.token_budget(1.0, 0.01)
    assert r["tokens_remaining"] == 100


def test_cascade_budget_has_levels() -> None:
    b = SigmaBudget()
    t = b.cascade_budget()
    assert "L1" in t and "L5" in t


def test_daily_cap_halt() -> None:
    b = SigmaBudget()
    b.daily_cap(10.0, spend_increment=3.0)
    st = b.daily_cap(5.0, spend_increment=3.0)
    assert st["halt"] is True


def test_sigma_roi() -> None:
    r = SigmaBudget.sigma_roi(100.0, 25.0)
    assert r["roi"] == 4.0
