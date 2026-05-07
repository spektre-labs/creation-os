# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from cos.tool_safety import ToolSafety


def test_blocked_rm_rf() -> None:
    ts = ToolSafety()
    r = ts.σ_before_execute("shell", "rm -rf /")
    assert r["classification"] == "BLOCKED"
    assert r["allow"] is False


def test_safe_ls() -> None:
    ts = ToolSafety()
    r = ts.σ_before_execute("shell", "ls -la")
    assert r["classification"] == "SAFE"
    assert r["allow"] is True


def test_moderate_pip_install() -> None:
    ts = ToolSafety()
    r = ts.σ_before_execute("shell", "pip install numpy")
    assert r["classification"] == "MODERATE"


def test_sigma_gates_safe_call() -> None:
    ts = ToolSafety()
    r = ts.σ_before_execute("read_file", "report.txt")
    assert r["sigma"] >= 0.0
    assert r["verdict"] in ("ACCEPT", "RETHINK", "ABSTAIN")


def test_audit_log_records() -> None:
    ts = ToolSafety()
    ts.σ_before_execute("shell", "ls")
    ts.σ_before_execute("shell", "rm -rf /")
    log = ts.get_audit_log()
    assert len(log) == 2
    assert log[1]["classification"] == "BLOCKED"


def test_approval_gate_callback() -> None:
    ts = ToolSafety()
    approved = ts.approval_gate(
        "shell",
        "pip install flask",
        user_callback=lambda t, a, r: True,
    )
    assert approved["allow"] is True


def test_custom_blocked_patterns() -> None:
    ts = ToolSafety(custom_blocked=["deploy_production"])
    r = ts.σ_before_execute("deploy", "deploy_production --force")
    assert r["classification"] == "BLOCKED"


class _GateAccept:
    def score(self, _p: str, _c: str):
        return 0.12, "ACCEPT"


class _GateAbstain:
    def score(self, _p: str, _c: str):
        return 0.95, "ABSTAIN"


def test_tier1_auto_approve() -> None:
    ts = ToolSafety(gate=_GateAccept())
    r = ts.check("read_file", {"path": "/tmp/x.txt"})
    assert r["tier"] == 1 and r["action"] == "APPROVE"


def test_tier2_sigma_decides() -> None:
    ts = ToolSafety(gate=_GateAccept())
    r = ts.check("write_file", {"path": "out.txt"})
    assert r["tier"] == 2 and r["action"] == "APPROVE" and "sigma" in r


def test_tier3_human_required() -> None:
    ts = ToolSafety()
    r = ts.check("delete_file", {"path": "x"})
    assert r["tier"] == 3 and r["action"] == "HUMAN_REQUIRED"


def test_unknown_tool_tier3() -> None:
    ts = ToolSafety()
    r = ts.check("unknown_tool_xyz", {})
    assert r["tier"] == 3 and r["action"] == "HUMAN_REQUIRED"


def test_register_custom_tool() -> None:
    ts = ToolSafety()
    ts.register_tool("my_safe_lookup", 1)
    assert ts.check("my_safe_lookup", {})["tier"] == 1


def test_audit_log_tracking() -> None:
    ts = ToolSafety(gate=_GateAccept())
    ts.check("read_file", {})
    ts.check("write_file", {})
    rep = ts.audit_log()
    assert rep["total_checks"] == 2
    assert rep["approved"] >= 2


def test_block_high_sigma() -> None:
    ts = ToolSafety(gate=_GateAbstain())
    r = ts.check("write_file", {"data": "x"})
    assert r["action"] == "BLOCK" and r.get("sigma", 0) >= 0.9
