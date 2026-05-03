# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""Tests for :mod:`cos.team`."""
from __future__ import annotations

from cos import SigmaGate
from cos.team import SigmaTeam


def test_team_assign() -> None:
    t = SigmaTeam(SigmaGate())
    a = t.assign("do X", ("analyst", "writer"))
    assert "analyst" in a and "writer" in a


def test_team_round_robin_picks_low_sigma() -> None:
    gate = SigmaGate()

    def good(_: str) -> str:
        return "4"

    def bad(_: str) -> str:
        return "purple elephant" * 40

    team = SigmaTeam(gate)
    team.register("analyst", good)
    team.register("critic", bad)
    r = team.round_robin("What is 2+2?", ["analyst", "critic"])
    assert r["scores"]["analyst"] < r["scores"]["critic"]
    assert r["picked"] == "analyst"


def test_team_proconductor_override_first() -> None:
    gate = SigmaGate()

    def a(_: str) -> str:
        return "a"

    def b(_: str) -> str:
        return "b answer long" * 3

    team = SigmaTeam(gate, proconductor_override=True)
    team.register("r1", b)
    team.register("r2", a)
    r = team.round_robin("q", ["r1", "r2"])
    assert r["picked"] == "r1"


def test_team_debate() -> None:
    gate = SigmaGate()

    def pro(q: str) -> str:
        return f"yes: {q[:10]}"

    def con(q: str) -> str:
        return "wrong " * 12

    team = SigmaTeam(gate)
    d = team.debate("capital france?", pro, con)
    assert d["winner"] in ("pro", "con", "tie")


def test_team_sigma_consensus_tight() -> None:
    c = SigmaTeam.sigma_consensus({"a": 0.21, "b": 0.22, "c": 0.2})
    assert c["consensus"] is True


def test_team_sigma_consensus_loose() -> None:
    c = SigmaTeam.sigma_consensus({"a": 0.1, "b": 0.9})
    assert c["consensus"] is False


