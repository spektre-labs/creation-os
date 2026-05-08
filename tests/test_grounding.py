# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.grounding import SigmaGrounding  # noqa: E402
from cos.sigma_gate import ABSTAIN, ACCEPT, RETHINK  # noqa: E402


class _GateLow:
    def score(self, a: str, b: str):  # noqa: ARG002
        return (0.05, ACCEPT)


class _GateHigh:
    def score(self, a: str, b: str):  # noqa: ARG002
        return (0.92, ABSTAIN)


class _GateHallucination:
    def score(self, a: str, b: str):  # noqa: ARG002
        return (0.9, ABSTAIN)


class _GateChain:
    def __init__(self) -> None:
        self._n = 0

    def score(self, a: str, b: str):  # noqa: ARG002
        self._n += 1
        if self._n == 2:
            return (0.88, ABSTAIN)
        return (0.06, ACCEPT)


class _GateStableDrift:
    def score(self, a: str, b: str):  # noqa: ARG002
        return (0.2, RETHINK)


class _GateUngroundedMix:
    def score(self, a: str, b: str):  # noqa: ARG002
        if str(a) == "ok":
            return (0.05, ACCEPT)
        return (0.9, ABSTAIN)


def test_ground_low_sigma() -> None:
    g = SigmaGrounding(gate=_GateLow())
    r = g.ground("cat", "furry animal")
    assert r["grounded"] is True
    assert r["σ"] < 0.15


def test_ground_high_sigma_not_grounded() -> None:
    g = SigmaGrounding(gate=_GateHigh())
    r = g.ground("foo", "bar")
    assert r["grounded"] is False
    assert r["verdict"] == ABSTAIN


def test_hallucination_check() -> None:
    g = SigmaGrounding(gate=_GateHallucination())
    h = g.hallucination_check("The moon is cheese", "Apollo samples are rock")
    assert h["hallucinating"] is True
    assert h["grounded"] is False


def test_ground_hierarchy_all_grounded() -> None:
    g = SigmaGrounding(gate=_GateLow())
    out = g.ground_hierarchy(
        [
            ("mammal", "warm-blooded vertebrate"),
            ("cat", "small mammal"),
        ]
    )
    assert out["all_grounded"] is True
    assert len(out["chain"]) == 2


def test_ground_hierarchy_weakest_link() -> None:
    g = SigmaGrounding(gate=_GateChain())
    out = g.ground_hierarchy([("a", "b"), ("c", "d"), ("e", "f")])
    assert out["weakest_link"] == "c"
    assert out["weakest_σ"] == 0.88
    assert out["all_grounded"] is False


def test_sensorimotor_ground() -> None:
    g = SigmaGrounding(gate=_GateLow())
    r = g.sensorimotor_ground("push", "block moved")
    assert r["embodied"] is True
    assert r["grounded"] is True


def test_drift_check_stable() -> None:
    g = SigmaGrounding(gate=_GateStableDrift())
    g.ground("slot", "value")
    d = g.drift_check("slot")
    assert d["drifted"] is False
    assert abs(d["drift"]) < 1e-9


def test_ungrounded_symbols_listed() -> None:
    g = SigmaGrounding(gate=_GateUngroundedMix())
    g.ground("x", "y")
    g.ground("ok", "ok_ref")
    bad = g.ungrounded_symbols()
    assert any(e["symbol"] == "x" for e in bad)
    assert not any(e["symbol"] == "ok" for e in bad)
