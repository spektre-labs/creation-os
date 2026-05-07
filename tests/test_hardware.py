# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import sys
from pathlib import Path
from unittest.mock import patch

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.hardware import HardwareInfo  # noqa: E402


def test_detect_returns_platform() -> None:
    info = HardwareInfo().detect()
    assert "platform" in info
    assert isinstance(info["platform"], str)
    assert len(info["platform"]) > 0


def test_recommendation_has_model() -> None:
    info = HardwareInfo().detect()
    rec = info.get("recommendation") or {}
    assert "model" in rec
    assert isinstance(rec["model"], str)
    assert len(rec["model"]) > 0


def test_apple_silicon_detection() -> None:
    with (
        patch("cos.hardware.subprocess.run", side_effect=FileNotFoundError()),
        patch("cos.hardware.platform.system", return_value="Darwin"),
        patch("cos.hardware.platform.machine", return_value="arm64"),
        patch("cos.hardware.platform.processor", return_value=""),
    ):
        info = HardwareInfo().detect()
    assert info["apple_silicon"] is True
    assert info["gpu"]["type"] == "apple_silicon"


def test_cpu_only_fallback() -> None:
    with (
        patch("cos.hardware.subprocess.run", side_effect=FileNotFoundError()),
        patch("cos.hardware.platform.system", return_value="Linux"),
        patch("cos.hardware.platform.machine", return_value="x86_64"),
        patch("cos.hardware.platform.processor", return_value="x86_64"),
    ):
        info = HardwareInfo().detect()
    assert info["apple_silicon"] is False
    assert info["gpu"]["type"] == "cpu_only"
