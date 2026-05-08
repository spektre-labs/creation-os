# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.noether import NoetherSigma  # noqa: E402


class _ConstGate:
    def __init__(self, sigma: float) -> None:
        self._s = float(sigma)

    def score(self, a: str, b: str):  # noqa: ARG002
        return (self._s, "ACCEPT")


class _LenGate:
    def score(self, a: str, b: str):  # noqa: ARG002
        L = len(str(b))
        return (min(0.95, 0.05 + 0.08 * L), "ACCEPT")


def test_lagrangian_at_zero() -> None:
    n = NoetherSigma()
    assert n.lagrangian(0.0) == 1.0


def test_lagrangian_at_one() -> None:
    n = NoetherSigma()
    assert n.lagrangian(1.0) == -1.0


def test_action_over_trajectory() -> None:
    n = NoetherSigma()
    s = n.action([0.0, 0.25, 0.5])
    assert s == round((1.0 + 0.5 + 0.0) / 3, 4)


def test_symmetry_preserved() -> None:
    n = NoetherSigma(gate=_ConstGate(0.2))
    r = n.symmetry_check(lambda x: x, ["a", "b", "c"])
    assert r["symmetric"] is True
    assert r["violations"] == []


def test_symmetry_broken() -> None:
    n = NoetherSigma(gate=_LenGate())
    r = n.symmetry_check(lambda x: str(x) + "!!!", ["x", "y"])
    assert r["symmetric"] is False
    assert len(r["violations"]) >= 1


def test_conservation_law_structure() -> None:
    n = NoetherSigma()
    d = n.conservation_law()
    for k in ("symmetry", "conserved_quantity", "lagrangian", "action", "equation_of_motion", "noether"):
        assert k in d
