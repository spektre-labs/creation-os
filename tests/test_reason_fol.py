# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.reason import SigmaReason  # noqa: E402
from cos.sigma_gate import SigmaGate  # noqa: E402


def test_parse_triple() -> None:
    r = SigmaReason()
    assert r.parse("France capital Paris") == ("France", "capital", "Paris")
    assert r.parse("short") is None


def test_unify_identical() -> None:
    r = SigmaReason()
    assert r.unify("a", "a") == {}


def test_unify_variable() -> None:
    r = SigmaReason()
    sub = r.unify("?x", "alpha")
    assert sub is not None
    assert sub.get("?x") == "alpha"


def test_unify_failure() -> None:
    r = SigmaReason()
    assert r.unify("foo", "bar") is None


def test_resolve_simple_fact() -> None:
    r = SigmaReason()
    gate = SigmaGate()
    kb = [("France", "capital", "Paris")]
    query = ("France", "capital", "?x")
    out = r.resolve(kb, query, gate, max_depth=5)
    assert out["proven"] is True
    assert len(out["steps"]) == 1
    assert out["verdict"] in ("ACCEPT", "RETHINK")


def test_resolve_sigma_per_step() -> None:
    class _HiGate:
        threshold_accept = 0.15
        threshold_abstain = 0.85

        def score(self, p: str, r: str) -> tuple[float, str]:
            _ = (p, r)
            return 0.92, "ABSTAIN"

    r = SigmaReason()
    kb = [("X", "p", "Y")]
    out = r.resolve(kb, ("X", "p", "?z"), _HiGate(), max_depth=5)
    assert out["proven"] is True
    assert out["total_σ"] >= 0.9
    assert out["verdict"] == "ABSTAIN"
