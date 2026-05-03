# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.code`."""
from __future__ import annotations

from cos.code import SigmaCode
from cos.sigma_gate import SigmaGate


def test_score_code_combined() -> None:
    c = SigmaCode()
    src = "def f():\n    return 1\n"
    r = c.score_code("add", src, SigmaGate())
    assert "sigma_combined" in r and r["syntax"]["ok"] is True


def test_syntax_error() -> None:
    r = SigmaCode.syntax_check("def x(", "python")
    assert r["ok"] is False


def test_static_analysis() -> None:
    big = "\n".join(["x=1"] * 250)
    s = SigmaCode.static_analysis(big)
    assert s["hits"] >= 1


def test_security_scan_eval() -> None:
    s = SigmaCode.security_scan("eval(input())")
    assert s["count"] >= 1


def test_test_generation() -> None:
    c = SigmaCode()
    r = c.test_generation("def g(): return 0\n", SigmaGate())
    assert "suggested_tests" in r


def test_sigma_per_function() -> None:
    c = SigmaCode()
    src = "def a():\n    return 1\n\ndef b():\n    return eval('1')\n"
    rows = c.sigma_per_function(src, SigmaGate())
    assert len(rows) >= 1


def test_non_python_syntax_skipped() -> None:
    r = SigmaCode.syntax_check("fn main() {}", "rust")
    assert r["ok"] is True
