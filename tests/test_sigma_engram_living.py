# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for ``SigmaEngram`` and ``SigmaLivingWeights`` (no ``torch`` dependency)."""
from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.sigma_engram import SigmaEngram  # noqa: E402
from cos.sigma_gate_core import Verdict  # noqa: E402
from cos.sigma_living_weights import SigmaLivingWeights  # noqa: E402


def test_sigma_living_weights_targets() -> None:
    lw = SigmaLivingWeights(16)
    assert lw.target_array(Verdict.ACCEPT) is None
    assert lw.target_array(Verdict.RETHINK) is lw.application
    assert lw.target_array(Verdict.ABSTAIN) is lw.firmware


def test_sigma_engram_accepts_only() -> None:
    mem = SigmaEngram(max_entries=100)
    mem.store("q1", "r1", Verdict.ACCEPT, sigma=0.1)
    mem.store("q2", "r2", Verdict.RETHINK, sigma=0.1)
    assert len(mem) == 1
    hits = mem.recall("q1", tau=0.5)
    assert len(hits) == 1 and hits[0][0] == "q1"
