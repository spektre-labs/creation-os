# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from cos.mega import Mega
from cos.sigma_gate import ABSTAIN


class _AlwaysAbstainGate:
    def score(self, _p: str, _r: str) -> tuple[float, str]:
        return (0.99, ABSTAIN)


def test_step_returns_all_stages() -> None:
    m = Mega()
    out = m.step("observe temperature is 21C", goal="stay safe")
    assert out["cycle"] == 1
    st = out["stages"]
    for key in (
        "perceive",
        "act",
        "learn",
    ):
        assert key in st
    assert "σ_cycle" in out


def test_step_abstains_on_high_sigma() -> None:
    m = Mega(gate=_AlwaysAbstainGate())
    out = m.step("anything")
    assert out["stages"]["act"]["action"] == "ABSTAIN"


def test_dream_consolidates() -> None:
    m = Mega()
    r = m.dream()
    assert isinstance(r, dict)
    if m._modules.get("world") is not None:
        assert "world" in r


def test_status_reports_modules() -> None:
    m = Mega()
    s = m.status()
    assert s["modules_total"] > 0
    assert s["modules_loaded"] >= 0
    assert isinstance(s["modules"], dict)
    assert "avg_σ" in s and "σ_trend" in s


def test_multi_cycle_sigma_tracking() -> None:
    m = Mega()
    m.step("a")
    m.step("b")
    assert len(m.σ_trace) >= 2
