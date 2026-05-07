# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.sigma_gate import ABSTAIN  # noqa: E402
from cos.transfer import SigmaTransfer  # noqa: E402


class _GateAnalogiesLow:
    """Always treat cross-domain primitive pairs as similar (σ < 0.5)."""

    def score(self, prompt: str, response: str):
        del prompt, response
        return 0.2, "ACCEPT"


class _GateAnalogiesHigh:
    """No pairs qualify as analogous."""

    def score(self, prompt: str, response: str):
        del prompt, response
        return 0.9, ABSTAIN


class _GatePullGrabPushNudge:
    """Bipartite primitive pairing: pull↔grab, push↔nudge (lab deterministic)."""

    def score(self, prompt: str, response: str):
        p, r = str(prompt), str(response)
        if "pull" in p and "grab" in r:
            return 0.2, "ACCEPT"
        if "push" in p and "nudge" in r:
            return 0.2, "ACCEPT"
        return 0.9, ABSTAIN


class _GateTransferOk:
    def score(self, prompt: str, response: str):
        del prompt, response
        return 0.1, "ACCEPT"


class _GateTransferFailRule:
    """Analogies qualify; rule transfer step ABSTAIN."""

    def score(self, prompt: str, response: str):
        if "rule in" in str(prompt):
            return 0.95, ABSTAIN
        return 0.2, "ACCEPT"


def test_register_domain() -> None:
    tr = SigmaTransfer()
    tr.register_domain("blocks", ["stack", "topple"], rules=["stack blocks"])
    assert "blocks" in tr.domains
    assert tr.domains["blocks"]["primitives"] == ["stack", "topple"]
    assert tr.domains["blocks"]["rules"] == ["stack blocks"]
    assert tr.domains["blocks"]["σ_avg"] == 0.5


def test_find_analogous_returns_pairs() -> None:
    tr = SigmaTransfer(gate=_GateAnalogiesLow())
    tr.register_domain("A", ["x", "y"])
    tr.register_domain("B", ["p", "q"])
    pairs = tr.find_analogous("A", "B")
    assert len(pairs) == 4
    assert all(p["σ"] < 0.5 for p in pairs)


def test_compose_novel_returns_sigma() -> None:
    tr = SigmaTransfer()
    out = tr.compose_novel(["pull", "twice"], operation="seq")
    assert "σ" in out
    assert isinstance(out["σ"], float)
    assert "seq" in out["composition"]
    assert out["primitives"] == ["pull", "twice"]


def test_compose_novel_coherent() -> None:
    tr = SigmaTransfer(gate=_GateTransferOk())
    out = tr.compose_novel(["a", "b"])
    assert out["verdict"] != ABSTAIN
    assert out["coherent"] is True


def test_transfer_maps_rule() -> None:
    tr = SigmaTransfer(gate=_GatePullGrabPushNudge())
    tr.register_domain("physics", ["pull", "push"])
    tr.register_domain("robot", ["grab", "nudge"])
    res = tr.transfer("physics", "robot", "pull and push")
    assert res["success"] is True
    assert "grab" in res["transferred_rule"]
    assert "nudge" in res["transferred_rule"]
    assert "pull" not in res["transferred_rule"]
    assert "push" not in res["transferred_rule"]


def test_transfer_no_analogies_fails() -> None:
    tr = SigmaTransfer(gate=_GateAnalogiesHigh())
    tr.register_domain("s", ["a"])
    tr.register_domain("t", ["b"])
    res = tr.transfer("s", "t", "rule")
    assert res["success"] is False
    assert res["reason"] == "no analogies found"


def test_transfer_score_calculation() -> None:
    tr = SigmaTransfer(gate=_GateTransferOk())
    tr.register_domain("a", ["x"])
    tr.register_domain("b", ["y"])
    tr.transfer("a", "b", "same")
    assert tr.transfer_score() == 1.0

    tr.transfer("a", "b", "again")
    assert tr.transfer_score() == 1.0

    tr2 = SigmaTransfer(gate=_GateTransferFailRule())
    tr2.register_domain("a", ["x"])
    tr2.register_domain("b", ["y"])
    tr2.transfer("a", "b", "z")
    assert tr2.transfer_score() == 0.0
    assert len(tr2.transfers) == 1
    assert tr2.transfers[0]["success"] is False
