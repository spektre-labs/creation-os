# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.governance`."""
from __future__ import annotations

from cos.governance import SigmaGovernance
from cos.sigma_gate import SigmaGate


def test_audit_log_append() -> None:
    g = SigmaGovernance(SigmaGate())
    r = g.audit_log(sigma=0.3, verdict="ACCEPT", prompt_hash="p", response_hash="q")
    assert r["row_hash"] and r["verdict"] == "ACCEPT"


def test_immutable_snapshot_is_copy() -> None:
    g = SigmaGovernance()
    g.audit_log(sigma=0.1, verdict="RETHINK", prompt_hash="a", response_hash="b")
    snap = g.immutable_snapshot()
    assert len(snap) == 1
    g.audit_log(sigma=0.2, verdict="ACCEPT", prompt_hash="c", response_hash="d")
    assert len(snap) == 1


def test_policy_engine_denied_user() -> None:
    g = SigmaGovernance()
    r = g.policy_engine({"deny_users": ["u1"]}, {"user": "u1", "tool": "x"})
    assert r["allowed"] is False and "user_denied" in r["violations"]


def test_compliance_report_counts() -> None:
    g = SigmaGovernance()
    ev = [{"verdict": "ACCEPT", "sigma": 0.1}, {"verdict": "RETHINK", "sigma": 0.6, "incident": True}]
    r = g.compliance_report(ev)
    assert r["counts"]["ACCEPT"] == 1 and r["counts"]["RETHINK"] == 1
    assert r["incidents"] == 1


def test_retention_policy() -> None:
    g = SigmaGovernance()
    g.retention_policy(days=30)
    assert g.retention_policy()["retention_days"] == 30


def test_right_to_explanation_pointer() -> None:
    g = SigmaGovernance()
    r = g.right_to_explanation("ACCEPT", 0.2)
    assert "explain" in r["explain_module"] and "/v1/explain" in r["endpoint"]
