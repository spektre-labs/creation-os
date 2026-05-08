# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

"""Tests for :class:`SigmaMoETokenRouter` / :class:`MoETokenExpert` in :mod:`cos.sigma_moe`."""
from __future__ import annotations

import sys
from pathlib import Path
from typing import Tuple

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.sigma_moe import MoETokenExpert, SigmaMoETokenRouter  # noqa: E402


class _BySpecialtyGate:
    """Lower σ when prompt mentions ``math`` or ``code``."""

    def score(self, prompt: str, response: str) -> Tuple[float, str]:
        p = str(prompt).lower()
        if "math" in p:
            return 0.11, "ACCEPT"
        if "code" in p:
            return 0.33, "ACCEPT"
        return 0.72, "RETHINK"


def test_add_expert() -> None:
    r = SigmaMoETokenRouter(gate=_BySpecialtyGate(), top_k=2)
    e = r.add_expert("algebra")
    assert isinstance(e, MoETokenExpert)
    assert e.id == 0
    assert len(r.experts) == 1


def test_route_selects_top_k() -> None:
    r = SigmaMoETokenRouter(gate=_BySpecialtyGate(), top_k=2)
    r.add_expert("math")
    r.add_expert("code")
    r.add_expert("poetry")
    out = r.route("solve", context="lab")
    assert len(out["selected_experts"]) == 2
    assert len(out["all_results"]) == 2


def test_route_lowest_sigma_best() -> None:
    r = SigmaMoETokenRouter(gate=_BySpecialtyGate(), top_k=1)
    r.add_expert("math")
    r.add_expert("poetry")
    out = r.route("x")
    assert out["best_expert"] == 0
    assert out["best_σ"] < 0.2


def test_load_balance_check() -> None:
    r = SigmaMoETokenRouter(gate=_BySpecialtyGate(), top_k=2)
    r.add_expert("math")
    r.add_expert("code")
    r.route("a")
    r.route("b")
    lb = r.load_balance()
    assert lb["total_tokens"] >= 2
    assert "imbalance" in lb and "balanced" in lb


def test_expert_utilization() -> None:
    r = SigmaMoETokenRouter(gate=_BySpecialtyGate(), top_k=1)
    r.add_expert("m1")
    r.add_expert("m2")
    r.route("t")
    u = r.expert_utilization()
    assert u["total"] == 2
    assert u["active"] >= 1
    assert 0 in u["dead"] or 1 in u["dead"] or u["active"] == 2


def test_dynamic_add_expert() -> None:
    r = SigmaMoETokenRouter(gate=_BySpecialtyGate(), top_k=1)
    r.add_expert("only")
    d = r.dynamic_add_expert([0.8, 0.82, 0.79], threshold=0.6)
    assert d.get("added") is True
    assert len(r.experts) == 2


def test_routing_weights_sum_to_one() -> None:
    r = SigmaMoETokenRouter(gate=_BySpecialtyGate(), top_k=2)
    r.add_expert("math")
    r.add_expert("code")
    out = r.route("z")
    s = sum(float(x["weight"]) for x in out["all_results"])
    assert abs(s - 1.0) < 1e-3
