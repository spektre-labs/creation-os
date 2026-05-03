# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.constitution`."""
from __future__ import annotations

from cos.constitution import SigmaConstitution
from cos.sigma_gate import SigmaGate


def test_principles_immutable_core() -> None:
    c = SigmaConstitution()
    assert "honesty" in c.principles
    assert "NOT_AGI_ACHIEVED" in c.immutable_core


def test_context_adapt_medical() -> None:
    c = SigmaConstitution()
    r = c.context_adapt("medical triage")
    assert r["weights"]["honesty"] >= r["weights"]["transparency"]


def test_check_returns_per_principle() -> None:
    c = SigmaConstitution(SigmaGate())
    r = c.check("2+2", "4")
    assert "sigma_per_principle" in r and isinstance(r["sigma_per_principle"], dict)


def test_record_override_blocks_immutable() -> None:
    c = SigmaConstitution()
    x = c.record_override("admin", {"removed_principles": ["NOT_AGI_ACHIEVED"]})
    assert x["ok"] is False


def test_record_override_allowed() -> None:
    c = SigmaConstitution()
    x = c.record_override("admin", {"weights": {"honesty": 0.5}})
    assert x["ok"] is True


def test_context_adapt_financial() -> None:
    c = SigmaConstitution()
    r = c.context_adapt("financial trading")
    assert sum(r["weights"].values()) == 1.0
