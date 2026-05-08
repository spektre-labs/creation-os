# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import pytest

from cos.social import AgentModel, SigmaSocialV2


def test_level_0_own_beliefs() -> None:
    s = SigmaSocialV2()
    r = s.level_0("reach agreement")
    assert r["level"] == 0
    assert "σ" in r and "verdict" in r


def test_level_1_model_other() -> None:
    s = SigmaSocialV2()
    s.observe_agent("alice", "wave")
    r = s.level_1("alice", "lobby")
    assert r["level"] == 1
    assert r["agent"] == "alice"
    assert r["n_observations"] >= 1


def test_level_2_model_their_model_of_me() -> None:
    s = SigmaSocialV2()
    r = s.level_2("bob", "meeting")
    assert r["level"] == 2
    assert "σ_meta" in r
    assert r["mutual_understanding"] in ("high", "moderate", "low")


def test_meta_tom_mutual_understanding() -> None:
    s = SigmaSocialV2()
    r = s.meta_tom(0.2, 0.21, 0.2)
    assert r["mutual_understanding"] == "high"
    assert "total_gap" in r and "insight" in r


def test_belief_resonance_detected(monkeypatch: pytest.MonkeyPatch) -> None:
    s = SigmaSocialV2()
    n = {"i": 0}

    def scores(_p: str, _r: str) -> tuple[float, str]:
        n["i"] += 1
        return (0.6 if n["i"] == 1 else 0.2, "ACCEPT")

    monkeypatch.setattr(s.gate, "score", scores)
    r = s.belief_resonance("carol", "win", "share")
    assert r["influenced"] is True
    assert r["resonance_strength"] > 0.1


def test_observe_updates_model() -> None:
    g = __import__("cos.sigma_gate", fromlist=["SigmaGate"]).SigmaGate()
    m = AgentModel("z", g)
    m.observe("a")
    m.observe("b")
    assert len(m.prediction_errors) >= 1


def test_cooperation_potential() -> None:
    s = SigmaSocialV2()
    r = s.cooperation_potential(["x", "y"])
    assert len(r["pairs"]) == 1
    assert "avg_cooperation_σ" in r


def test_recursive_depth() -> None:
    s = SigmaSocialV2()
    assert s.tom_depth == 0
    s.level_1("d", "")
    assert s.tom_depth >= 1
    s.level_2("d", "")
    assert s.tom_depth >= 2
