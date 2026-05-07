# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from cos.social import SigmaSocial


def test_model_agent_from_interactions() -> None:
    soc = SigmaSocial()
    model = soc.model_agent(
        "agent-1",
        [{"action": "trade", "outcome": "success", "σ": 0.2}],
    )
    assert model["known"] is True
    assert model["interactions"] == 1


def test_trust_from_history() -> None:
    soc = SigmaSocial()
    m = soc.model_agent(
        "ally",
        [
            {"outcome": "success", "σ": 0.1},
            {"outcome": "success", "σ": 0.1},
        ],
    )
    assert m["trust"] > 0.5


def test_predict_unknown_agent_abstains() -> None:
    soc = SigmaSocial()
    out = soc.predict_intent("stranger", "meeting")
    assert out["prediction"] is None
    assert out["verdict"] == "ABSTAIN"


def test_predict_trusted_agent() -> None:
    soc = SigmaSocial()
    soc.model_agent(
        "friend",
        [{"outcome": "success", "σ": 0.05}, {"outcome": "success", "σ": 0.05}],
    )
    out = soc.predict_intent("friend", "negotiate split")
    assert out["prediction"] == "cooperative"
    assert out["agent"] == "friend"


def test_meta_tom_high_understanding() -> None:
    soc = SigmaSocial()
    r = soc.meta_tom(0.2, 0.22, 0.21)
    assert r["mutual_understanding"] == "high"


def test_meta_tom_low_understanding() -> None:
    soc = SigmaSocial()
    r = soc.meta_tom(0.1, 0.9, 0.05)
    assert r["mutual_understanding"] == "low"


class _SimilarGate:
    def score(self, a: str, b: str):
        sa, sb = str(a).lower(), str(b).lower()
        if "goal" in sa and "goal" in sb:
            return 0.1, "ACCEPT"
        return 0.9, "RETHINK"


def test_shared_goals_found() -> None:
    soc = SigmaSocial(gate=_SimilarGate())
    pairs = soc.shared_goal(["team goal"], ["shared goal"], alignment_threshold=0.3)
    assert len(pairs) >= 1
    assert pairs[0]["alignment_σ"] < 0.3
