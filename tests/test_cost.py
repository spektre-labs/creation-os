# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from cos.cost import SigmaCost
from cos.fleet import SigmaFleet
from cos.sigma_gate import SigmaGate


def test_cost_per_query_positive() -> None:
    c = SigmaCost()
    r = c.cost_per_query("gpt-4o", 1000, cascade_level=2)
    assert r["eur"] >= 0.0


def test_cascade_savings_fraction() -> None:
    c = SigmaCost()
    s = c.cascade_savings(0.7)
    assert s["l1_fraction"] == 0.7


def test_fleet_routing_cost() -> None:
    c = SigmaCost()
    f = SigmaFleet()
    f.register("local", cost=0.01)
    g = SigmaGate()
    r = c.fleet_routing_cost(f, "hello", g, tokens_if_routed=100)
    assert "eur" in r


def test_budget_and_report() -> None:
    c = SigmaCost()
    c.budget(10.0)
    c.record_usage(1.5)
    rep = c.report()
    assert rep["total_eur"] > 0


def test_projection_days() -> None:
    c = SigmaCost()
    p = c.projection(7, 3.0)
    assert p["projected_eur"] == 21.0


def test_comparison_has_disclaimer() -> None:
    c = SigmaCost()
    x = c.comparison(queries=10, tokens_per_query=200)
    assert "disclaimer" in x
