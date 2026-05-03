# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.device`."""
from __future__ import annotations

from pathlib import Path

from cos.device import SigmaDevice


def test_load_manual_txt(tmp_path: Path) -> None:
    d = SigmaDevice()
    p = tmp_path / "m.txt"
    p.write_text(
        "Intro paragraph.\n\nDifferential lock: press switch for 2s.\n\n--- page 3 ---\n\nMore text.\n\n",
        encoding="utf-8",
    )
    r = d.load_manual(p)
    assert r["ok"] is True and r["n_chunks"] >= 1


def test_query_device(tmp_path: Path) -> None:
    d = SigmaDevice()
    p = tmp_path / "oven.txt"
    p.write_text("Set temperature using knob A. Child lock: hold 3s.\n\n", encoding="utf-8")
    d.load_manual(p)
    q = d.query_device("How do I set temperature?", "appliance")
    assert "verdict" in q and "sigma" in q


def test_context_policy_automotive() -> None:
    pol = SigmaDevice.context_policy("automotive")
    assert pol.get("safety") == "max" and pol.get("latency_target_ms") == 50.0


def test_sigma_threshold_stricter_auto() -> None:
    assert SigmaDevice.sigma_threshold_per_device("automotive") < SigmaDevice.sigma_threshold_per_device("appliance")


def test_action_classification_control() -> None:
    d = SigmaDevice()
    c = d.action_classification("unlock the charge port", "ev")
    assert c["class"] == "control" and c["requires_confirmation"] is True


def test_fallback_hint() -> None:
    h = SigmaDevice.fallback_hint(12)
    assert "12" in h and "manual" in h.lower()
