# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.edge`."""
from __future__ import annotations

from cos.edge import SigmaEdge
from cos.sigma_gate import SigmaGate


def test_deploy_profile_phone() -> None:
    p = SigmaEdge.deploy_profile("phone")
    assert p["battery_aware"] is True and p["max_model_mb"] == 500


def test_deploy_profile_mcu_llm_false() -> None:
    p = SigmaEdge.deploy_profile("mcu")
    assert p.get("llm") is False


def test_model_select_cap() -> None:
    e = SigmaEdge()
    r = e.model_select("reason about math", {"max_model_mb": 300})
    assert "model" in r


def test_quantize_phone_q4() -> None:
    q = SigmaEdge.quantize_for_device(None, {"device_type": "phone"})
    assert q["quantization"] == "Q4_K_M"


def test_sigma_gate_overhead_disclaimer() -> None:
    o = SigmaEdge.sigma_gate_overhead("phone")
    assert "disclaimer" in o


def test_test_time_compute() -> None:
    e = SigmaEdge()

    class M:
        def generate(self, p: str) -> str:
            return "a" if "ttc:0" in p else "What is 2+2? answer: 4"

    out = e.test_time_compute("qq", M(), SigmaGate(), difficulty="hard")
    assert out["n"] >= 2


def test_battery_budget_skip_levels() -> None:
    b = SigmaEdge.battery_budget(15.0, 3)
    assert b["skip_probe_levels_l2_l5"] is True
