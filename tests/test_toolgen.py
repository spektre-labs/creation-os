# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.genesis.toolgen` (runtime tool registration lab)."""
from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.genesis.toolgen import SigmaToolGen  # noqa: E402


class _NeedHigh:
    def score(self, _p: str, _r: str):
        return 0.72, "RETHINK"


class _LowAccept:
    def __init__(self, sigma: float = 0.12) -> None:
        self._σ = float(sigma)

    def score(self, _p: str, _r: str):
        return self._σ, "ACCEPT"


class _Abstain:
    def score(self, _p: str, _r: str):
        return 0.95, "ABSTAIN"


class _PickTool:
    """Lower σ when problem mentions 'sum' and intent mentions 'add'."""

    def score(self, prompt: str, response: str):
        p, r = str(prompt).lower(), str(response).lower()
        if "sum" in p and "add" in r:
            return 0.12, "ACCEPT"
        if "sum" in p and "sort" in r:
            return 0.82, "RETHINK"
        return 0.5, "ACCEPT"


class _EvolveGate:
    def score(self, _intent: str, code: str):
        if "return 2" in str(code):
            return 0.08, "ACCEPT"
        return 0.48, "ACCEPT"


def test_need_tool_high_sigma() -> None:
    g = SigmaToolGen(gate=_NeedHigh())
    n = g.need_tool("compute optimal rocket trajectory")
    assert n["need"] is True
    assert n["σ"] > 0.5


def test_create_valid_tool() -> None:
    g = SigmaToolGen(gate=_LowAccept(0.11))
    code = """
def double(x):
    return int(x) * 2
"""
    r = g.create("double", code, "double a number")
    assert r["created"] is True
    assert "double" in g.tools


def test_create_rejects_unsafe() -> None:
    g = SigmaToolGen(gate=_LowAccept())
    r = g.create("bad", "import os\nos.system('ls')\ndef bad():\n    pass", "test")
    assert r["created"] is False
    assert "BLOCKED" in r["reason"]


def test_create_rejects_high_sigma() -> None:
    g = SigmaToolGen(gate=_Abstain())
    code = "def f(): return 1\n"
    r = g.create("f", code, "constant one")
    assert r["created"] is False
    assert "ABSTAIN" in r["reason"]


def test_use_tool_returns_result() -> None:
    g = SigmaToolGen(gate=_LowAccept(0.1))
    code = "def add(a, b):\n    return int(a) + int(b)\n"
    g.create("add", code, "add two integers")
    u = g.use("add", 2, 3)
    assert u.get("error") is None
    assert u["result"] == 5


def test_find_tool_matches() -> None:
    g = SigmaToolGen(gate=_PickTool())
    g.create(
        "adder",
        "def adder(a,b):\n    return int(a)+int(b)\n",
        "add two numbers",
    )
    g.create(
        "sorter",
        "def sorter(xs):\n    return sorted(xs)\n",
        "sort a list",
    )
    found = g.find_tool("need to sum invoices quickly")
    assert found is not None
    assert found["tool"] == "adder"


def test_evolve_tool_improves() -> None:
    g = SigmaToolGen(gate=_EvolveGate())
    old = "def fx():\n    return 1\n"
    g.create("fx", old, "return small int")
    new = "def fx():\n    return 2\n"
    out = g.evolve_tool("fx", new)
    assert out.get("evolved") is True
    assert out["σ_new"] <= out["σ_old"]
    assert g.use("fx")["result"] == 2
