# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.theory.fitness import SigmaFitness  # noqa: E402


class _ConstGate:
    def __init__(self, sigma: float) -> None:
        self._s = float(sigma)

    def score(self, a: str, b: str):  # noqa: ARG002
        return (self._s, "ACCEPT")


class _PrefixGate:
    def score(self, env: str, g: str):  # noqa: ARG002
        s = str(g)
        return (0.25 if s.startswith("good") else 0.75, "ACCEPT")


class _LenGate:
    """Longer genotypes → lower σ (toy landscape)."""

    def score(self, env: str, g: str):  # noqa: ARG002
        L = len(str(g))
        sigma = max(0.05, min(0.95, 0.88 - 0.04 * L))
        return (sigma, "ACCEPT")


def test_fitness_is_one_minus_sigma() -> None:
    sf = SigmaFitness(gate=_ConstGate(0.2))
    r = sf.fitness("x")
    assert r["fitness"] == 0.8
    assert r["σ"] == 0.2


def test_populate_ranks_by_fitness() -> None:
    sf = SigmaFitness(gate=_PrefixGate())
    sf.populate(["bad1", "good1", "bad2", "good2"], "")
    assert str(sf.population[0]["genotype"]).startswith("good")


def test_select_keeps_top() -> None:
    sf = SigmaFitness(gate=_PrefixGate())
    sf.populate(["a", "good", "b", "c", "good2"], "")
    surv = sf.select(0.4)
    assert len(surv) >= 1
    assert all(str(x["genotype"]).startswith("good") for x in surv[:2])


def test_vary_produces_variant() -> None:
    sf = SigmaFitness()
    v = sf.vary("abc")
    assert v != "abc"


def test_evolve_reduces_sigma() -> None:
    sf = SigmaFitness(gate=_LenGate())
    out = sf.evolve(
        ["a", "b"],
        generations=8,
        top_fraction=0.5,
        mutation_fn=lambda g: str(g) + "x",
    )
    assert out["improved"] is True
    assert out["σ_end"] < out["σ_start"]


def test_fundamental_theorem_string() -> None:
    sf = SigmaFitness(gate=_ConstGate(0.5))
    out = sf.evolve(["u", "v"], generations=1)
    assert "variance" in out["fundamental_theorem"].lower()
