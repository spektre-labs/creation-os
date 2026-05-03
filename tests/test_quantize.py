# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from cos.quantize import SigmaQuantize


def test_layer_bits_order() -> None:
    q = SigmaQuantize()
    assert q.layer_bits(0.1) < q.layer_bits(0.9)


def test_mixed_map() -> None:
    q = SigmaQuantize()
    m = q.mixed_precision_map({"a": 0.1, "b": 0.9})
    assert set(m.keys()) == {"a", "b"}


def test_aggressive_for_low_sigma() -> None:
    assert SigmaQuantize.layer_bits(0.1) <= 4


def test_conservative_high_sigma() -> None:
    q = SigmaQuantize()
    assert q.layer_bits(0.95) >= 8


def test_map_stable() -> None:
    q = SigmaQuantize()
    m1 = q.mixed_precision_map({"L0": 0.5})
    m2 = q.mixed_precision_map({"L0": 0.5})
    assert m1 == m2
