# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from cos.evolve import SigmaEvolve


class _GateByMarker:
    def score(self, _prompt: str, response: str) -> tuple[float, str]:
        s = str(response)
        if "IMPROVED" in s:
            return (0.08, "ACCEPT")
        if "BADREG" in s:
            return (0.96, "RETHINK")
        return (0.55, "ACCEPT")


def test_propose_accepts_improvement() -> None:
    ev = SigmaEvolve(gate=_GateByMarker(), risk_budget=0.1)
    r = ev.propose("mod", "legacy code", "IMPROVED patch")
    assert r["accepted"] is True
    assert float(r["Δσ"]) > 0
    assert r["reason"] == "σ improved"


def test_propose_rejects_regression() -> None:
    ev = SigmaEvolve(gate=_GateByMarker(), risk_budget=0.05)
    r = ev.propose("mod", "calm code IMPROVED small", "BADREG worse")
    assert r["accepted"] is False
    assert "REJECTED" in r["reason"]


def test_evolve_loop_converges() -> None:
    ev = SigmaEvolve(gate=_GateByMarker(), risk_budget=0.1)
    comps = {"a": "base"}

    def _same(_n: str, code: str) -> str:
        return str(code)

    out = ev.evolve_loop(comps, _same, max_generations=4)
    assert out["convergence"] is True
    assert any(h.get("converged") for h in out["history"] if isinstance(h, dict))
    assert comps["a"] == "base"


def test_safety_invariants_listed() -> None:
    ev = SigmaEvolve()
    inv = ev.safety_invariants()
    assert "immutable" in inv and "mutable" in inv and "principle" in inv
    assert any("sigma_gate.h" in str(x).lower() for x in inv["immutable"])
    assert "NOT AGI ACHIEVED" in inv["immutable"]


def test_godel_check_structure() -> None:
    ev = SigmaEvolve()
    g = ev.godel_check()
    assert g["godel"] == "system cannot prove own consistency"
    assert "σ_self" in g
    assert 0.0 <= float(g["σ_self"]) <= 1.0


def test_risk_budget_tracking() -> None:
    ev = SigmaEvolve(gate=_GateByMarker(), risk_budget=0.05)
    ev.propose("x", "IMPROVED", "BADREG")
    ev.propose("y", "plain", "IMPROVED")
    sr = ev.scrivens_risk()
    assert sr["rejections"] >= 1
    assert sr["improvements"] >= 1
    assert "total_σ_gained" in sr and "budget_used" in sr


def test_archive_stores_history() -> None:
    ev = SigmaEvolve(gate=_GateByMarker(), risk_budget=0.1)
    n0 = len(ev.archive)
    ev.propose("c", "old", "IMPROVED new")
    assert len(ev.archive) == n0 + 1
    assert ev.archive[-1]["component"] == "c"
