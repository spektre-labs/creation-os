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
