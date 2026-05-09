# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Smoke tests for :mod:`cos.agi_demo`."""
from __future__ import annotations

from cos.agi_demo import run_demo


def test_agi_demo_prints_banner(capsys) -> None:
    run_demo()
    out = capsys.readouterr().out
    assert "COGNITIVE LOOP DEMO" in out
    assert "NOT AGI" in out
    assert "NOT AGI ACHIEVED" in out
