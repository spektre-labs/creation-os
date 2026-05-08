# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.category import Morphism, Object, SigmaFunctor  # noqa: E402
from cos.sigma_gate import ACCEPT  # noqa: E402


class _GateConst:
    def __init__(self, sigma: float) -> None:
        self._s = float(sigma)

    def score(self, prompt: str, payload: str):  # noqa: ARG002
        return (self._s, ACCEPT)


class _GateCompose:
    def score(self, prompt: str, payload: str):  # noqa: ARG002
        p = str(prompt)
        if "compose" in p:
            return (0.41, ACCEPT)
        if p == "part":
            return (0.40, ACCEPT)
        return (0.9, ACCEPT)


class _GateFaithful:
    """Distinct σ at two decimals for sequential map_morphism calls."""

    def __init__(self) -> None:
        self._i = 0

    def score(self, prompt: str, payload: str):  # noqa: ARG002
        self._i += 1
        return (0.10 + self._i * 0.02, ACCEPT)


def test_object_creation() -> None:
    o = Object("X", state="payload")
    assert o.name == "X"
    assert o.state == "payload"
    assert "X" in repr(o)


def test_morphism_isomorphism() -> None:
    a, b = Object("A"), Object("B")
    low = Morphism(a, b, sigma=0.05)
    high = Morphism(a, b, sigma=0.5)
    assert low.is_isomorphism() is True
    assert high.is_isomorphism() is False


def test_functor_map_object() -> None:
    F = SigmaFunctor(gate=_GateConst(0.02))
    d = Object("D", state="idea")
    m = F.map_object(d)
    assert m["isomorphic"] is True
    assert m["σ"] < 0.1
    assert "R(D)" == m["realized"].name


def test_functor_map_morphism() -> None:
    F = SigmaFunctor(gate=_GateConst(0.15))
    a, b = Object("a"), Object("b")
    f = Morphism(a, b, name="f", sigma=0.0)
    out = F.map_morphism(f)
    assert out["realized"].name == "R(f)"
    assert out["structure_preserved"] is True
    assert abs(out["σ"] - 0.15) < 1e-6


def test_functor_faithful() -> None:
    empty = SigmaFunctor(gate=_GateConst(0.1))
    assert empty.is_faithful() is True

    F = SigmaFunctor(gate=_GateFaithful())
    o1, o2, o3 = Object("1"), Object("2"), Object("3")
    F.map_morphism(Morphism(o1, o2, name="m1"))
    F.map_morphism(Morphism(o2, o3, name="m2"))
    assert F.is_faithful() is True

    G = SigmaFunctor(gate=_GateConst(0.333))
    G.map_morphism(Morphism(o1, o2, name="a"))
    G.map_morphism(Morphism(o2, o3, name="b"))
    assert G.is_faithful() is False


def test_composition_preserved() -> None:
    F = SigmaFunctor(gate=_GateCompose())
    a, b, c = Object("A"), Object("B"), Object("C")
    f = Morphism(a, b, name="f", sigma=0.0)
    g = Morphism(b, c, name="g", sigma=0.0)
    chk = F.composition_check(f, g)
    assert chk["composition_preserved"] is True


def test_natural_transformation_smooth() -> None:
    F = SigmaFunctor()
    traj = [0.10, 0.11, 0.12]
    n = F.natural_transformation_sigma(traj)
    assert n["natural"] is True
    assert n["smooth"] is True
    n2 = F.natural_transformation_σ(traj)
    assert n2["natural"] is n["natural"]
