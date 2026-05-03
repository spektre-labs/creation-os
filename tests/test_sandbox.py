# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.sandbox`."""
from __future__ import annotations

from cos.sandbox import SigmaSandbox
from cos.sigma_gate import SigmaGate


def test_execute_safe_simple_expression() -> None:
    sb = SigmaSandbox(SigmaGate())
    r = sb.execute_safe("1 + 2", timeout_s=1.0, memory_mb=64)
    assert r["ok"] is True and r["result"] == 3


def test_execute_safe_rejects_multistatement() -> None:
    sb = SigmaSandbox(SigmaGate())
    r = sb.execute_safe("1+1;2+2")
    assert r["ok"] is False


def test_sigma_before_execute() -> None:
    sb = SigmaSandbox(SigmaGate())
    p = sb.sigma_before_execute("import os")
    assert "sigma" in p


def test_result_sigma() -> None:
    sb = SigmaSandbox(SigmaGate())
    r = sb.result_sigma("hello world")
    assert "verdict" in r


def test_isolation_flags() -> None:
    sb = SigmaSandbox()
    assert sb.filesystem_isolation()["host_fs_access"] is False
    assert sb.network_isolation()["outbound"] is False


def test_rollback_on_failure_clears_scratch() -> None:
    sb = SigmaSandbox(SigmaGate())
    sb.execute_safe("2 * 3")
    assert sb._scratch.get("last_result") == 6
    sb._last_ok = False
    sb.rollback_on_failure()
    assert sb._scratch == {}
