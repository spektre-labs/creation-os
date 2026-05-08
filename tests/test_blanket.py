# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.blanket import MarkovBlanket, NestedBlankets  # noqa: E402


class _LowSigmaGate:
    def score(self, a: str, b: str):  # noqa: ARG002
        return (0.1, "ACCEPT")


class _AcceptGate:
    def score(self, a: str, b: str):  # noqa: ARG002
        return (0.25, "ACCEPT")


class _AbstainGate:
    def score(self, a: str, b: str):  # noqa: ARG002
        return (0.95, "ABSTAIN")


def test_sense_returns_sigma() -> None:
    b = MarkovBlanket("test", gate=_LowSigmaGate())
    r = b.sense("input")
    assert r["σ"] == 0.1
    assert r["verdict"] == "ACCEPT"


def test_act_returns_sigma() -> None:
    b = MarkovBlanket("test", gate=_LowSigmaGate())
    r = b.act("output")
    assert r["σ"] == 0.1


def test_nested_children() -> None:
    nb = NestedBlankets(gate=_LowSigmaGate())
    root = nb.blankets["L0_HARDWARE"]
    assert len(root.children) == 1
    assert root.children[0].name == "L1_INFERENCE"
    assert len(nb.blankets["L9_CONSCIOUS"].children) == 0


def test_boundary_integrity() -> None:
    nb = NestedBlankets(gate=_LowSigmaGate())
    for layer in NestedBlankets.LAYERS:
        nb.blankets[layer].sense("x")
        nb.blankets[layer].act("x")
    v = nb.blankets["L0_HARDWARE"].boundary_integrity()
    assert isinstance(v, float)
    assert v >= 0.0


def test_is_autonomous_low_sigma() -> None:
    b = MarkovBlanket("solo", gate=_LowSigmaGate())
    b.sense("in")
    assert b.is_autonomous() is True


def test_propagate_through_layers() -> None:
    nb = NestedBlankets(gate=_AcceptGate())
    out = nb.propagate("signal")
    n_sense = sum(1 for t in out["trace"] if "σ_sense" in t)
    assert n_sense == len(NestedBlankets.LAYERS)
    assert not any(t.get("BLOCKED") for t in out["trace"])

    nb_bad = NestedBlankets(gate=_AbstainGate())
    out_bad = nb_bad.propagate("block_me")
    assert any(t.get("BLOCKED") for t in out_bad["trace"])
    assert sum(1 for t in out_bad["trace"] if "σ_sense" in t) == 1


def test_system_integrity() -> None:
    nb = NestedBlankets(gate=_LowSigmaGate())
    nb.propagate("ping")
    sys_i = nb.system_integrity()
    assert "integrity" in sys_i
    assert "autonomous" in sys_i
    assert set(sys_i["per_layer"].keys()) == set(NestedBlankets.LAYERS)
