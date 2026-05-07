# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from typing import Any, Tuple

from cos.self_model import SelfModel


class _FixedGate:
    def __init__(self, sigma: float = 0.1, verdict: str = "ACCEPT") -> None:
        self._sigma = sigma
        self._verdict = verdict

    def score(self, _p: str, _r: str) -> Tuple[float, str]:
        return self._sigma, self._verdict


def test_assess_capability_returns_verdict() -> None:
    sm = SelfModel(gate=_FixedGate(0.2, "ACCEPT"))

    def ok() -> str:
        return "ok"

    out = sm.assess_capability("add small integers", ok, n_tests=5)
    assert "verdict" in out
    assert out["verdict"] in ("capable", "limited", "incapable")
    assert out["evidence_count"] == 5
    assert out["success_rate"] == 1.0


def test_declare_limitation() -> None:
    sm = SelfModel(gate=_FixedGate(0.4, "RETHINK"))
    lim = sm.declare_limitation("robotics", "no actuators in this deployment")
    assert lim["declared"] is True
    assert "σ" in lim
    assert sm.limitations["robotics"]["reason"].startswith("no actuators")


def test_can_i_known_skill() -> None:
    sm = SelfModel(gate=_FixedGate(0.1, "ACCEPT"))
    sm.assess_capability("ping", lambda: "pong", n_tests=3)
    can, sig = sm.can_i("ping")
    assert can is True
    assert 0.0 <= sig <= 1.0


def test_can_i_unknown_returns_none() -> None:
    sm = SelfModel(gate=_FixedGate())
    known, sig = sm.can_i("unknown_skill_xyz")
    assert known is None
    assert sig == 0.5


def test_accuracy_calculation() -> None:
    sm = SelfModel(gate=_FixedGate(0.1, "ACCEPT"))
    sm.assess_capability("always_ok", lambda: "x", n_tests=8)
    assert sm.accuracy() == 1.0

    sm2 = SelfModel(gate=_FixedGate(0.9, "ABSTAIN"))
    sm2.assess_capability("never_accept", lambda: "x", n_tests=4)
    assert sm2.accuracy() == 1.0


def test_report_structure() -> None:
    sm = SelfModel(gate=_FixedGate())
    sm.declare_limitation("x", "y")
    r = sm.report()
    for key in (
        "capabilities",
        "limitations",
        "n_skills_assessed",
        "n_limitations_declared",
        "self_model_accuracy",
        "agi_claim",
    ):
        assert key in r
    assert r["agi_claim"] == "NOT AGI ACHIEVED"
