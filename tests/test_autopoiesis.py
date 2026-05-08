# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.autopoiesis import Autopoietic  # noqa: E402


class _AcceptGate:
    def score(self, a: str, b: str):  # noqa: ARG002
        return (0.2, "ACCEPT")


class _AbstainGate:
    def score(self, a: str, b: str):  # noqa: ARG002
        return (0.95, "ABSTAIN")


class _RepairGate:
    """First call (produce) moderate σ; repair path returns lower σ."""

    def __init__(self) -> None:
        self._n = 0

    def score(self, a: str, b: str):  # noqa: ARG002
        if str(a).startswith("repair"):
            return (0.2, "ACCEPT")
        self._n += 1
        return (0.85, "ACCEPT")


def test_produce_component() -> None:
    ap = Autopoietic(gate=_AcceptGate())
    out = ap.produce("mod_a", lambda: "payload")
    assert out["produced"] is True
    assert "mod_a" in ap.components


def test_produce_rejected_high_sigma() -> None:
    ap = Autopoietic(gate=_AbstainGate())
    out = ap.produce("x", lambda: "y")
    assert out["produced"] is False
    assert "reason" in out
    assert len(ap.components) == 0


def test_maintain_repairs_degraded() -> None:
    ap = Autopoietic(gate=_RepairGate())
    ap.components["fragile"] = {
        "value": "data",
        "σ": 0.9,
        "sigma": 0.9,
        "generation": 0,
    }
    m = ap.maintain()
    assert m["repaired"] >= 1
    assert ap.components["fragile"]["σ"] < 0.9


def test_couple_preserves_identity() -> None:
    ap = Autopoietic(gate=_AcceptGate())
    c = ap.couple("environment tick")
    assert c["coupled"] is True
    assert c["identity_preserved"] is True


def test_couple_rejects_boundary_threat() -> None:
    ap = Autopoietic(gate=_AbstainGate())
    c = ap.couple("hostile flux")
    assert c["coupled"] is False
    assert c["identity_preserved"] is True
    assert "reason" in c


def test_is_alive_with_components() -> None:
    ap = Autopoietic(gate=_AcceptGate())
    ap.produce("k", lambda: 1)
    st = ap.is_alive()
    assert st["alive"] is True
    assert st["components"] == 1


def test_is_dead_without_components() -> None:
    ap = Autopoietic(gate=_AcceptGate())
    st = ap.is_alive()
    assert st["alive"] is False
    assert st["components"] == 0
