# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from cos.reason import (
    Clause,
    Const,
    Func,
    Predicate,
    SigmaReason,
    Var,
    fol_parse,
    prove_by_refutation,
    prove_by_refutation_with_sigma,
    resolve,
    unify,
)


def test_unify_constants() -> None:
    s = unify(Const("a"), Const("a"))
    assert s is not None


def test_unify_var_const() -> None:
    s = unify(Var("x"), Const("a"))
    assert s is not None
    assert s["x"] == Const("a")


def test_unify_fail() -> None:
    s = unify(Const("a"), Const("b"))
    assert s is None


def test_unify_func() -> None:
    f1 = Func("f", [Var("x"), Const("b")])
    f2 = Func("f", [Const("a"), Const("b")])
    s = unify(f1, f2)
    assert s is not None
    assert s["x"] == Const("a")


def test_resolve_complementary() -> None:
    c1 = Clause([Predicate("P", [Const("a")])])
    c2 = Clause([Predicate("P", [Const("a")], negated=True)])
    resolvents = resolve(c1, c2)
    assert len(resolvents) > 0
    assert resolvents[0].is_empty()


def test_prove_simple() -> None:
    clauses = [
        Clause([Predicate("P", [Const("a")])]),
        Clause(
            [
                Predicate("P", [Const("a")], negated=True),
                Predicate("Q", [Const("a")]),
            ]
        ),
    ]
    goal = Predicate("Q", [Const("a")])
    result = prove_by_refutation(clauses, goal)
    assert result["proved"]


def test_consistency_ok() -> None:
    reason = SigmaReason()
    result = reason.check_consistency(
        [
            ("capital", ["france", "paris"]),
            ("capital", ["germany", "berlin"]),
        ],
        uniqueness=["capital"],
    )
    assert result["consistent"]
    assert result["sigma"] < 0.3


def test_consistency_conflict() -> None:
    reason = SigmaReason()
    result = reason.check_consistency(
        [
            ("capital", ["france", "paris"]),
            ("capital", ["france", "london"]),
        ],
        uniqueness=["capital"],
    )
    assert not result["consistent"]
    assert result["sigma"] >= 0.5
    assert len(result["conflicts"]) > 0


def test_direct_contradiction() -> None:
    reason = SigmaReason()
    result = reason.check_consistency(
        [
            ("alive", ["socrates"]),
            ("not_alive", ["socrates"]),
        ]
    )
    assert not result["consistent"]


def test_fol_parse_single_literal() -> None:
    clauses = fol_parse("P(a)")
    assert len(clauses) == 1
    lit = next(iter(clauses[0].literals))
    assert lit.name == "P" and not lit.negated


def test_fol_parse_negated() -> None:
    clauses = fol_parse("not Q(b)")
    lit = next(iter(clauses[0].literals))
    assert lit.negated


def test_prove_with_sigma_trace() -> None:
    from cos.sigma_gate import SigmaGate

    gate = SigmaGate()
    clauses = fol_parse("P(a)\nnot P(a) | Q(a)")
    goal = Predicate("Q", [Const("a")])
    out = prove_by_refutation_with_sigma(clauses, goal, gate, max_steps=50)
    assert out.get("proved") is True
    assert "step_sigmas" in out
    assert len(out["step_sigmas"]) == len(out["trace"])
