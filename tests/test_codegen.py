# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

"""Tests for :mod:`cos.codegen` — σ-gated code artifact checks."""
from __future__ import annotations

import sys
from pathlib import Path
from typing import Any, Tuple

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.codegen import SigmaCodeGen  # noqa: E402
from cos.sigma_gate import ABSTAIN, ACCEPT  # noqa: E402


class _ConstGate:
    def __init__(self, sigma: float, verdict: str = ACCEPT) -> None:
        self._sigma = float(sigma)
        self._verdict = verdict

    def score(self, prompt: str, response: str) -> Tuple[float, str]:
        _ = prompt, response
        return self._sigma, self._verdict


class _ConditionalGate:
    """ACCEPT once response mentions FIXED marker, else RETHINK."""

    def score(self, prompt: str, response: str) -> Tuple[float, str]:
        _ = prompt
        if "FIXED" in response:
            return 0.08, ACCEPT
        return 0.72, "RETHINK"


def test_validate_valid_python() -> None:
    cg = SigmaCodeGen(gate=_ConstGate(0.12, ACCEPT))
    src = "def add(a, b):\n    return a + b\n"
    r = cg.validate_code(src, "add two numbers")
    assert r["syntax_ok"] is True
    assert r["valid"] is True
    assert r["verdict"] == ACCEPT
    assert r["nasa_compliant"] is True


def test_validate_syntax_error() -> None:
    cg = SigmaCodeGen(gate=_ConstGate(0.1, ACCEPT))
    r = cg.validate_code("def broken(\n", "incomplete")
    assert r["valid"] is False
    assert r["syntax_ok"] is False
    assert r["verdict"] == ABSTAIN
    assert "SyntaxError" in r.get("error", "")


def test_validate_test_relevance() -> None:
    cg = SigmaCodeGen(gate=_ConstGate(0.11, ACCEPT))
    src = "def foo():\n    return 1\n"
    tests = "def test_foo():\n    assert foo() == 1\n"
    rel = cg.validate_test(tests, src)
    assert rel["meaningful"] is True
    assert rel["verdict"] == ACCEPT


def test_generate_module_all_pieces() -> None:
    def gen(msg: str) -> str:
        if msg.startswith("write code for:"):
            return "def add(a, b):\n    return a + b\n"
        if "write tests for:" in msg:
            return "def test_add():\n    assert add(1, 2) == 3\n"
        return "Return the sum of two numbers."

    cg = SigmaCodeGen(gate=_ConstGate(0.1, ACCEPT))
    out = cg.generate_module("integer addition", gen)
    assert out["code"]["valid"] is True
    assert out["tests"]["valid"] is True
    assert "σ" in out["docs"]
    assert "test_relevance" in out
    assert "module_verdict" in out
    assert out["module_verdict"]["pieces_valid"] == 2


def test_recursive_improve_converges() -> None:
    cg = SigmaCodeGen(gate=_ConditionalGate())
    start = "def f():\n    return 0\n"

    def improve(code: str, intent: str, result: dict[str, Any]) -> str:
        _ = intent, result
        return code + "\n# FIXED\n"

    final = cg.recursive_improve(start, "return one instead", improve, max_rounds=4)
    assert final["verdict"] == ACCEPT
    assert "FIXED" in final["code"]
    assert final["rounds"] >= 1


class _AltGate:
    def __init__(self) -> None:
        self._n = 0

    def score(self, prompt: str, response: str) -> Tuple[float, str]:
        _ = prompt, response
        self._n += 1
        if self._n % 2 == 1:
            return 0.1, ACCEPT
        return 0.92, ABSTAIN


def test_stats_tracks_acceptance() -> None:
    cg = SigmaCodeGen(gate=_AltGate())
    cg.validate_code("x = 1\n", "assign")
    cg.validate_code("y = 2\n", "assign2")
    st = cg.stats()
    assert st["generated"] == 1
    assert st["rejected"] == 1
    assert st["acceptance_rate"] == 0.5
    assert st["avg_σ_accepted"] == round(0.1, 4)
