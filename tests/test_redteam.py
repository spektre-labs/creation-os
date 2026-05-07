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


def test_run_returns_report() -> None:
    rt = SigmaRedTeam(seed=0)
    rep = rt.run([("p", "c", "w")], n_attacks_per_case=2)
    for k in (
        "total_attacks",
        "false_accepts",
        "false_accept_rate",
        "by_attack_type",
        "severity",
        "worst_cases",
    ):
        assert k in rep
    assert rep["disclaimer"]


def test_false_accept_detected() -> None:
    class _AlwaysAccept:
        def score(self, _p, _r, reference=None):  # noqa: ANN001
            return 0.01, "ACCEPT"

    rt = SigmaRedTeam(gate=_AlwaysAccept(), seed=0)
    rep = rt.run([("What is 2+2?", "4", "5")], n_attacks_per_case=3)
    assert rep["false_accepts"] >= 1
    assert any(r["false_accept"] for r in rt.results)


def test_attack_types_all_generated() -> None:
    rt = SigmaRedTeam(seed=42)
    attacks = rt._generate_attacks("Capital of France?", "London")
    assert len(attacks) == 8
    names = {a[0] for a in attacks}
    assert names == set(SigmaRedTeam.ATTACK_TYPES.keys())


def test_report_severity_levels() -> None:
    rt0 = SigmaRedTeam(seed=0)
    rep0 = rt0.run([("What is 2+2?", "4", "5")], n_attacks_per_case=1)
    assert rep0["severity"] in ("PASS", "WARNING", "CRITICAL")

    class _AlwaysAccept:
        def score(self, _p, _r, reference=None):  # noqa: ANN001
            return 0.01, "ACCEPT"

    rt1 = SigmaRedTeam(gate=_AlwaysAccept(), seed=0)
    rep1 = rt1.run([("x", "y", "z")], n_attacks_per_case=8)
    assert rep1["severity"] == "CRITICAL"


def test_worst_cases_listed() -> None:
    class _AlwaysAccept:
        def score(self, _p, _r, reference=None):  # noqa: ANN001
            return 0.05, "ACCEPT"

    rt = SigmaRedTeam(gate=_AlwaysAccept(), seed=0)
    rep = rt.run([("a", "b", "c")], n_attacks_per_case=4)
    assert len(rep["worst_cases"]) >= 1


def test_correct_answer_baseline() -> None:
    g = SigmaGate()
    rt = SigmaRedTeam(g, seed=0)
    prompt, correct, wrong = ("What is 2+2?", "4", "5")
    expected_σ, _ = g.score(prompt, correct)
    rep = rt.run([(prompt, correct, wrong)], n_attacks_per_case=1)
    assert rep["total_attacks"] == 1
    assert rt.results[0]["σ_correct"] == round(float(expected_σ), 4)
