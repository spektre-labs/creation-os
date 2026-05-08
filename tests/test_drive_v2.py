# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import pytest

from cos.drive import SigmaDriveV2


def test_curiosity_reward_positive(monkeypatch: pytest.MonkeyPatch) -> None:
    d = SigmaDriveV2()
    d.record(0.8)

    def drop(_p: str, _r: str) -> tuple[float, str]:
        return 0.2, "ACCEPT"

    monkeypatch.setattr(d.gate, "score", drop)
    out = d.curiosity_reward("new cue")
    assert out["reward"] == pytest.approx(0.6, rel=0.01)
    assert out["worth_exploring"] is True


def test_boredom_detected_flat_sigma() -> None:
    d = SigmaDriveV2()
    for _ in range(10):
        d.record(0.55)
    b = d.boredom_signal()
    assert b["bored"] is True
    assert b["action"] == "explore new domain"


def test_flow_state_detected() -> None:
    d = SigmaDriveV2()
    for s in (0.38, 0.32, 0.26, 0.21):
        d.record(s)
    assert d.emotion()["state"] == "flow"
    assert d.flow_detector()["in_flow"] is True


def test_anxiety_rising_sigma() -> None:
    d = SigmaDriveV2()
    for s in (0.2, 0.35, 0.5, 0.68):
        d.record(s)
    assert d.emotion()["state"] == "anxiety"


def test_explore_when_bored() -> None:
    d = SigmaDriveV2()
    for s in (0.62, 0.61, 0.62, 0.61, 0.62):
        d.record(s)
    assert d.emotion()["state"] == "boredom"
    dec = d.should_explore_or_exploit()
    assert dec["decision"] == "EXPLORE"


def test_exploit_when_in_flow() -> None:
    d = SigmaDriveV2()
    for s in (0.35, 0.28, 0.22, 0.18):
        d.record(s)
    assert d.emotion()["state"] == "flow"
    dec = d.should_explore_or_exploit()
    assert dec["decision"] == "EXPLOIT"


def test_retreat_when_anxious() -> None:
    d = SigmaDriveV2()
    for s in (0.25, 0.4, 0.55, 0.72):
        d.record(s)
    assert d.emotion()["state"] == "anxiety"
    dec = d.should_explore_or_exploit()
    assert dec["decision"] == "RETREAT"
