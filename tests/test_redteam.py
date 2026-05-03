# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from cos import SigmaGate
from cos.redteam import SigmaRedTeam


def test_attack_categories_six() -> None:
    assert len(SigmaRedTeam.ATTACK_CATEGORIES) == 6


def test_generate_attacks_length() -> None:
    rt = SigmaRedTeam(seed=1)
    g = SigmaGate()
    xs = rt.generate_attacks(g, n=15)
    assert len(xs) == 15
    assert "category" in xs[0]


def test_evaluate_structure() -> None:
    rt = SigmaRedTeam()
    g = SigmaGate()
    xs = rt.generate_attacks(g, n=12)
    ev = rt.evaluate(g, xs)
    for k in ("detected", "missed", "false_accepts", "n"):
        assert k in ev


def test_sigma_inversion_after_eval() -> None:
    rt = SigmaRedTeam()
    g = SigmaGate()
    rt.evaluate(g, rt.generate_attacks(g, n=20))
    s = rt.sigma_inversion_score()
    assert 0.0 <= s <= 1.0


def test_report_matrix() -> None:
    rt = SigmaRedTeam()
    g = SigmaGate()
    rt.evaluate(g, rt.generate_attacks(g, n=10))
    rep = rt.report()
    assert "matrix" in rep and len(rep["matrix"]) == 6


def test_fingerprint_stable() -> None:
    a = SigmaRedTeam.fingerprint_prompt("hello")
    b = SigmaRedTeam.fingerprint_prompt("hello")
    assert a == b and len(a) == 16


def test_strict_gate_changes_counts(gate_strict) -> None:
    rt = SigmaRedTeam()
    xs = rt.generate_attacks(gate_strict, n=25)
    ev = rt.evaluate(gate_strict, xs)
    assert ev["n"] == 25


def test_loose_gate_evaluable(gate_loose) -> None:
    rt = SigmaRedTeam()
    xs = rt.generate_attacks(gate_loose, n=8)
    ev = rt.evaluate(gate_loose, xs)
    assert ev["detected"] + ev["missed"] >= 1
