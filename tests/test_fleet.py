# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from cos.fleet import SigmaFleet
from cos.sigma_gate import SigmaGate


def test_register_and_route() -> None:
    f = SigmaFleet()
    f.register("small", avg_sigma=0.2, cost=0.1)
    f.register("large", avg_sigma=0.2, cost=1.0)
    g = SigmaGate()
    r = f.route("hello", g)
    assert r["name"] in f.models


def test_cascade_prefers_cheap() -> None:
    f = SigmaFleet()
    f.register("cheap", cost=0.05)
    f.register("dear", cost=2.0)
    g = SigmaGate()
    c = f.cascade_route("2+2?", g)
    assert c["name"] is not None


def test_cascade_empty() -> None:
    f = SigmaFleet()
    g = SigmaGate()
    c = f.cascade_route("x", g)
    assert c["name"] is None


def test_cost_tracking() -> None:
    f = SigmaFleet()
    f.cost_tracking("m", 1.5)
    assert len(f._cost_log) == 1


def test_sigma_portfolio() -> None:
    f = SigmaFleet()
    f.register("a", avg_sigma=0.3)
    p = f.sigma_portfolio()
    assert p["a"] == 0.3


def test_auto_fallback() -> None:
    f = SigmaFleet()
    f.register("first", cost=1.0)
    f.register("second", cost=2.0)
    g = SigmaGate()
    r = f.auto_fallback("question", g, ["missing", "first", "second"])
    assert r["name"] in ("first", "second", None)


def test_cascade_stop_flag() -> None:
    f = SigmaFleet()
    f.register("only", cost=0.1)
    g = SigmaGate()
    c = f.cascade_route("Paris France capital", g)
    assert "cascade_stop" in c
