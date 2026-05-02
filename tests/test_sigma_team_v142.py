# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""v142 σ-team workspace: shared Engram, RBAC, budgets, audit."""
from __future__ import annotations

from typing import Any, List, Optional

from cos.engram_v2 import EngramV2
from cos.sigma_gate_core import Verdict
from cos.sigma_rbac import SigmaRBAC
from cos.sigma_team import SigmaTeam


class _ConstGate:
    def score(self, prompt: str, response: str) -> tuple[float, Verdict]:
        _ = prompt, response
        return 0.08, Verdict.ACCEPT


class _CtxModel:
    def generate(self, prompt: str, *, context: Optional[List[Any]] = None) -> str:
        bits: List[str] = []
        for item in context or []:
            if isinstance(item, dict) and item.get("content"):
                bits.append(str(item["content"]))
        blob = " | ".join(bits)
        return f"SYNTH:{blob[:600]}::Q:{prompt[:120]}"


def test_two_users_shared_context_surfaces_both_memories() -> None:
    eg = EngramV2(gate=None)
    eg.store("Q3 revenue was 42M USD (internal flash).", "cfo", 0.05, Verdict.ACCEPT)
    eg.store("Alice agreed the 42M figure in the board prep doc.", "alice", 0.06, Verdict.ACCEPT)
    team = SigmaTeam(_ConstGate(), eg, model=_CtxModel(), rbac=SigmaRBAC())
    team.add_member("bob", "developer")
    out = team.query("bob", "What was Q3 revenue and what did Alice say in the board prep doc?")
    text = str(out.get("response", ""))
    assert "42M" in text
    assert "Alice" in text or "alice" in text.lower()


def test_analyst_query_accept_but_no_write_skips_engram() -> None:
    eg = EngramV2(gate=None)
    team = SigmaTeam(_ConstGate(), eg, model=_CtxModel(), rbac=SigmaRBAC())
    team.add_member("ann", "analyst")
    n0 = len(eg.layers["episodic"].all())
    out = team.query("ann", "Summarize Q3 narrative.")
    assert "error" not in out
    n1 = len(eg.layers["episodic"].all())
    assert n0 == n1


def test_developer_accept_persists_to_engram() -> None:
    eg = EngramV2(gate=None)
    team = SigmaTeam(_ConstGate(), eg, model=_CtxModel(), rbac=SigmaRBAC())
    team.add_member("dev", "developer")
    n0 = len(eg.layers["episodic"].all())
    team.query("dev", "Record that Q4 planning starts Monday.")
    n1 = len(eg.layers["episodic"].all())
    assert n1 > n0


def test_viewer_cannot_query_without_query_permission() -> None:
    eg = EngramV2(gate=None)
    team = SigmaTeam(_ConstGate(), eg, model=_CtxModel(), rbac=SigmaRBAC())
    team.add_member("vi", "viewer")
    out = team.query("vi", "What is the budget?")
    assert out.get("error")
    assert "permission" in str(out.get("error", "")).lower()


def test_sigma_budget_exhausted_blocks_query() -> None:
    eg = EngramV2(gate=None)
    team = SigmaTeam(_ConstGate(), eg, model=_CtxModel(), rbac=SigmaRBAC())
    team.add_member("maxed", "analyst")
    team.members["maxed"]["used"] = 2000
    team.members["maxed"]["sigma_budget"] = 2000
    out = team.query("maxed", "One more question")
    assert out.get("error")
    assert "budget" in str(out.get("error", "")).lower()


def test_rbac_permission_check() -> None:
    rbac = SigmaRBAC()
    u = rbac.member_record("viewer")
    assert rbac.check_permission(u, "read")
    assert not rbac.check_permission(u, "write")
