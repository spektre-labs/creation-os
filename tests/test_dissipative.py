# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.dissipative import DissipativeStructure  # noqa: E402


class _DecreasingGate:
    def __init__(self) -> None:
        self._v = 0.62

    def score(self, a: str, b: str):  # noqa: ARG002
        self._v = max(0.05, self._v - 0.02)
        return (self._v, "ACCEPT")


def test_distance_from_equilibrium() -> None:
    d = DissipativeStructure()
    assert d.measure_distance_from_equilibrium() == 0.5
    d.sigma_trace = [0.4, 0.4]
    assert d.measure_distance_from_equilibrium() == 0.6


def test_fluctuation_probability_large_unlikely() -> None:
    d = DissipativeStructure()
    small = d.fluctuation_probability(0.05)
    large = d.fluctuation_probability(0.8)
    assert small > large


def test_entropy_rate_calculation() -> None:
    d = DissipativeStructure()
    d.sigma_trace = [0.1, 0.2, 0.3]
    r = d.entropy_rate(window=5)
    assert r == 0.1


def test_is_dissipative_ordered() -> None:
    d = DissipativeStructure()
    d.sigma_trace = [0.6, 0.58, 0.56, 0.54, 0.52, 0.5]
    st = d.is_dissipative()
    assert st["dissipative"] is True
    assert "distance_from_equilibrium" in st


def test_is_not_dissipative_when_dissolving() -> None:
    d = DissipativeStructure()
    d.sigma_trace = [0.3, 0.35, 0.4, 0.45, 0.5, 0.55]
    st = d.is_dissipative()
    assert st["dissipative"] is False


def test_step_tracks_alive() -> None:
    d = DissipativeStructure(gate=_DecreasingGate())
    out = None
    for i in range(14):
        out = d.step(f"x{i}")
    assert out is not None
    assert out["alive"] is True

