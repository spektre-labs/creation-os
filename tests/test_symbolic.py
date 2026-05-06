# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.symbolic import (  # noqa: E402
    SigmaSymbolic,
    Term,
    apply_bindings,
    unify,
)


def test_term_creation() -> None:
    t = Term("foo", [Term("a"), Term("?X")])
    assert t.name == "foo"
    assert len(t.args) == 2
    assert repr(t) == "foo(a, ?X)"


def test_variable_detection() -> None:
    assert Term("?X").is_variable is True
    assert Term("x").is_variable is False


def test_unify_constants_equal() -> None:
    a = Term("a")
    assert unify(a, a, {}) == {}


def test_unify_constants_different_fails() -> None:
    assert unify(Term("a"), Term("b"), {}) is None


def test_unify_variable_binds() -> None:
    u = unify(Term("?X"), Term("k42"), {})
    assert u is not None
    assert u["?X"] == Term("k42")


def test_unify_compound_terms() -> None:
    u = unify(
        Term("pair", [Term("?X"), Term("b")]),
        Term("pair", [Term("a"), Term("?Y")]),
        {},
    )
    assert u is not None
    assert apply_bindings(Term("?X"), u) == Term("a")
    assert apply_bindings(Term("?Y"), u) == Term("b")
    assert unify(Term("f", [Term("?X")]), Term("f", [Term("g", [Term("?X")])]), {}) is None


def test_fact_query_simple() -> None:
    eng = SigmaSymbolic()
    eng.add_fact("rainy", "seattle")
    eng.add_fact("cold", "seattle")
    out = eng.query("rainy", "seattle")
    assert out["solutions"] and all("?X" not in s for s in out["solutions"])
    out2 = eng.query("rainy", "?C")
    cities = {str(apply_bindings(Term("?C"), sol)) for sol in out2["solutions"]}
    assert cities == {"seattle"}


def test_rule_backward_chaining() -> None:
    eng = SigmaSymbolic()
    eng.add_fact("parent", "u", "v")
    eng.add_rule("ancestor", ["?X", "?Y"], [("parent", ["?X", "?Y"])])
    out = eng.query("ancestor", "?A", "?B")
    pairs = {(str(s["?A"]), str(s["?B"])) for s in out["solutions"]}
    assert ("u", "v") in pairs


def test_grandparent_rule() -> None:
    eng = SigmaSymbolic()
    eng.add_fact("parent", "alice", "bob")
    eng.add_fact("parent", "bob", "carol")
    eng.add_rule(
        "grandparent",
        ["?X", "?Z"],
        [
            ("parent", ["?X", "?Y"]),
            ("parent", ["?Y", "?Z"]),
        ],
    )
    out = eng.query("grandparent", "alice", "?G")
    grandchildren = {str(apply_bindings(Term("?G"), sol)) for sol in out["solutions"]}
    assert grandchildren == {"carol"}


def test_sigma_per_inference_step() -> None:
    class CountGate:
        def __init__(self) -> None:
            self.i = 0

        def score(self, prompt: str, response: str) -> tuple[float, str]:
            self.i += 1
            return float(self.i) * 0.05, "ACCEPT"

    g = CountGate()
    eng = SigmaSymbolic(gate=g)
    eng.add_fact("edge", "1", "2")
    eng.add_fact("edge", "1", "3")
    out = eng.query("edge", "1", "?Y")
    sigmas = [step["σ"] for step in out["trace"] if step.get("type") == "fact"]
    assert sigmas == [0.05, 0.1]
    assert eng.σ_proof(out["trace"]) == max(sigmas)
