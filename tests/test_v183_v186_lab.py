# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""v183–v186 lab tests: few-shot, symbolic engine, optional FastAPI import."""
from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.sigma_fewshot import HashEmbeddingEncoder, SigmaFewShot  # noqa: E402
from cos.sigma_symbolic import SigmaSymbolic, parse_goal, parse_rule  # noqa: E402
from cos.sigma_twin import TwinGateLab, TwinModelLab  # noqa: E402


def test_sigma_fewshot_learn_predict() -> None:
    gate, model = TwinGateLab(), TwinModelLab(style="short")
    fs = SigmaFewShot(gate, model, HashEmbeddingEncoder(dim=16))
    out = fs.learn(
        "fi_en",
        [("Hei", "Hello"), ("Kiitos", "Thank you"), ("Näkemiin", "Goodbye")],
    )
    assert out.get("learned") is True
    pred = fs.predict("fi_en", "Huomenta")
    assert "response" in pred and "sigma" in pred


def test_sigma_symbolic_grandparent() -> None:
    gate = TwinGateLab()
    s = SigmaSymbolic(gate)
    s.assert_fact_goal(parse_goal("parent(tom,bob)"))
    s.assert_fact_goal(parse_goal("parent(bob,ann)"))
    s.assert_rule_dict(parse_rule("grandparent(X,Z) :- parent(X,Y), parent(Y,Z)"))
    sols = s.query("grandparent(tom,X)")
    assert sols
    b0 = sols[0]["bindings"]
    assert str(s.substitute("X", b0)) == "ann"


def test_sigma_symbolic_resolution_demo() -> None:
    gate = TwinGateLab()
    s = SigmaSymbolic(gate)
    c1 = [SigmaSymbolic.literal("human", ["X"], neg=True), SigmaSymbolic.literal("mortal", ["X"], neg=False)]
    c2 = [SigmaSymbolic.literal("human", ["socrates"], neg=False)]
    r = s.resolution(c1, c2)
    assert r is not None
    assert any(str(lit.get("pred")) == "mortal" for lit in r["resolvent"])


def test_api_serve_import_guard() -> None:
    try:
        from cos.api import serve as serve_mod  # noqa: F401
    except ImportError:
        return
    assert hasattr(serve_mod, "app")
