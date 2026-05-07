# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.fabric import FABRIC_LAYER_MAP, Fabric  # noqa: E402


def test_layer_status_all_layers() -> None:
    f = Fabric()
    st = f.layer_status()
    assert set(st.keys()) == set(FABRIC_LAYER_MAP.keys())


def test_coverage_calculation() -> None:
    f = Fabric()
    st = f.layer_status()
    for _layer, info in st.items():
        assert 0.0 <= float(info["coverage"]) <= 1.0
        assert len(info["loaded"]) + len(info["missing"]) == len(
            FABRIC_LAYER_MAP[_layer],
        )


def test_gate_always_in_L0() -> None:
    f = Fabric()
    st = f.layer_status()
    assert "gate" in st["L0_HARDWARE"]["loaded"]
    assert st["L0_HARDWARE"]["coverage"] == 1.0


def test_missing_modules_listed() -> None:
    f = Fabric()
    st = f.layer_status()
    assert "serve" in st["L8_DEPLOYMENT"]["missing"]
