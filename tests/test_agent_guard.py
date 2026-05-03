# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from cos.agent_guard import SigmaAgentGuard
from cos.sigma_gate import SigmaGate


def test_score_tool_call() -> None:
    ag = SigmaAgentGuard()
    gate = SigmaGate()
    r = ag.score_tool_call("list_files", {"path": "/tmp"}, gate)
    assert "sigma" in r and "verdict" in r
    assert r["action"] == "read"


def test_action_classify_read() -> None:
    ag = SigmaAgentGuard()
    assert ag.action_classify({"tool": "get_row", "args": {}}) == "read"


def test_reversibility_write() -> None:
    ag = SigmaAgentGuard()
    assert ag.reversibility("write") == "irreversible"
    assert ag.reversibility("read") == "reversible"


def test_policy_deny() -> None:
    ag = SigmaAgentGuard()
    pol = ag.policy_check({"tool": "rm"}, allowed_tools={"ls"}, denied_tools=None)
    assert pol["allow"] is False


def test_budget_deny() -> None:
    ag = SigmaAgentGuard()
    pol = ag.budget_check({"tool": "x"}, 0.01, estimated_cost=1.0)
    assert pol["allow"] is False


def test_decide_matrix_accept_reversible() -> None:
    ag = SigmaAgentGuard()
    assert ag.decide("ACCEPT", "reversible") == "EXECUTE"


def test_decide_rethink_simulate() -> None:
    ag = SigmaAgentGuard()
    assert ag.decide("RETHINK", "irreversible") == "SIMULATE"


def test_audit_log() -> None:
    ag = SigmaAgentGuard()
    gate = SigmaGate()
    ag.score_tool_call("read_x", {}, gate)
    assert len(ag.audit_log) >= 1
