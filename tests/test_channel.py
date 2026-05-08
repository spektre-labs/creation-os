# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.channel import SigmaChannel  # noqa: E402


class _ConstGate:
    def __init__(self, sigma: float) -> None:
        self._s = float(sigma)

    def score(self, a: str, b: str):  # noqa: ARG002
        return (self._s, "ACCEPT")


def test_capacity_zero_noise_is_one() -> None:
    ch = SigmaChannel()
    assert ch.capacity(0.0) == 1.0


def test_capacity_max_noise_is_zero() -> None:
    ch = SigmaChannel()
    assert ch.capacity(0.5) == 0.0
    assert ch.capacity(1.0) == 0.0


def test_transmit_returns_quality() -> None:
    ch = SigmaChannel(gate=_ConstGate(0.15))
    out = ch.transmit("hello world", context="ctx")
    assert "σ" in out and 0.0 < out["capacity"] <= 1.0
    assert out["verdict"] == "ACCEPT"
    assert len(ch.transmissions) == 1


def test_mutual_information() -> None:
    ch = SigmaChannel(gate=_ConstGate(0.2))
    mi = ch.mutual_information([("a", "b"), ("c", "d")])
    assert mi == ch.capacity(0.2)
    assert SigmaChannel.sigma_from_mi_ratio(1.0, 1.0) == 0.0
    assert SigmaChannel.sigma_from_mi_ratio(1.0, 0.0) == 1.0
    assert SigmaChannel.sigma_from_mi_ratio(1.0, 0.5) == 0.5


def test_rate_distortion() -> None:
    ch = SigmaChannel(gate=_ConstGate(0.8))
    rd = ch.rate_distortion(["one two", "three"], target_sigma=0.2)
    assert rd["messages"] == 2
    assert rd["total_bits"] >= rd["messages"]


def test_shannon_limit() -> None:
    ch = SigmaChannel(gate=_ConstGate(0.3))
    ch.transmit("m1")
    ch.transmit("m2")
    lim = ch.shannon_limit()
    assert lim["channel_capacity"] == ch.capacity(0.3)
    assert "shannon_says" in lim
