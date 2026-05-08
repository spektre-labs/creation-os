# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

"""Tests for :mod:`cos.scaling` — σ vs compute power-law fit (lab)."""
from __future__ import annotations

import re
import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.scaling import SigmaScaling  # noqa: E402


class _SynthGate:
    """σ decreases with compute parsed from ``prompt`` (``compute=...``)."""

    def score(self, prompt: str, response: str):
        m = re.search(r"compute=([0-9.eE+-]+)", str(prompt))
        c = float(m.group(1)) if m else 1.0
        c = max(c, 0.5)
        σ = 0.18 + 0.72 * (c ** -0.38)
        return min(0.94, max(0.19, float(σ))), "ACCEPT"


def test_measure_records() -> None:
    s = SigmaScaling(gate=_SynthGate())
    assert len(s.measurements) == 0
    s.measure(10.0, "compute=10", "x")
    assert len(s.measurements) == 1
    assert s.measurements[0]["compute"] == 10.0
    assert "σ" in s.measurements[0]


def test_fit_law_returns_params() -> None:
    s = SigmaScaling(gate=_SynthGate())
    for c in (2.0, 5.0, 8.0, 15.0, 30.0, 60.0):
        s.measure(c, f"compute={c}", "resp")
    law = s.fit_law()
    assert "error" not in law
    assert "A" in law and "α" in law and "E" in law
    assert law["α"] > 0


def test_predict_σ_decreases_with_compute() -> None:
    s = SigmaScaling(gate=_SynthGate())
    for c in (3.0, 6.0, 12.0, 24.0, 48.0, 96.0):
        s.measure(c, f"compute={c}", "resp")
    law = s.fit_law()
    hi = s.predict_σ(4.0, law)["σ_predicted"]
    lo = s.predict_σ(90.0, law)["σ_predicted"]
    assert lo < hi


def test_compute_needed_for_target() -> None:
    s = SigmaScaling(gate=_SynthGate())
    for c in (2.0, 4.0, 8.0, 16.0, 32.0, 64.0):
        s.measure(c, f"compute={c}", "resp")
    law = s.fit_law()
    mid = 0.45
    out = s.compute_needed(mid, law)
    assert "compute_needed" in out
    assert out["compute_needed"] != float("inf")
    assert out["compute_needed"] > 0


def test_efficiency_frontier() -> None:
    s = SigmaScaling(gate=_SynthGate())
    for c in (4.0, 8.0, 16.0, 32.0, 64.0):
        s.measure(c, f"compute={c}", "resp")
    ef = s.efficiency_frontier(n_points=5)
    assert "error" not in ef
    assert "frontier" in ef and len(ef["frontier"]) >= 2
    assert "law" in ef and "insight" in ef
