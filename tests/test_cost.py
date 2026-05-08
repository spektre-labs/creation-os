# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from cos.cost import CostManager, SigmaCost
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


class _GateCheap:
    def score(self, _p: str, _r: str):
        return 0.1, "ACCEPT"


class _GatePremium:
    def score(self, _p: str, _r: str):
        return 0.95, "ACCEPT"


def test_select_model_low_sigma_local() -> None:
    cm = CostManager(gate=_GateCheap())
    r = cm.select_model("easy prompt")
    assert r["model"] == "local_3b"
    assert r["sigma_pre"] < 0.2


def test_select_model_high_sigma_opus() -> None:
    cm = CostManager(gate=_GatePremium())
    r = cm.select_model("hard prompt")
    assert r["model"] == "claude_opus"


def test_record_tracks_cost() -> None:
    cm = CostManager(budget=100.0)
    e = cm.record("gpt4o_mini", 1_000_000, 500_000)
    assert e["cost"] > 0 and cm.spent > 0


def test_budget_exhausted_forces_local() -> None:
    cm = CostManager(gate=_GatePremium(), budget=0.0)
    cm.spent = 0.0
    cm.budget = 0.0
    r = cm.select_model("x", sigma_pre=0.99)
    assert r["model"] == "local_7b"


def test_summary_by_model() -> None:
    cm = CostManager(budget=50.0)
    cm.record("gpt4o_mini", 1000, 1000)
    cm.record("local_7b", 5000, 5000)
    s = cm.summary()
    assert s["total_calls"] == 2
    assert "gpt4o_mini" in s["by_model"] and "local_7b" in s["by_model"]


def test_savings_vs_opus() -> None:
    cm = CostManager(budget=1000.0)
    cm.record("gpt4o_mini", 2_000_000, 1_000_000)
    s = cm.summary()
    assert s["savings_vs_all_opus_percent"] > 0.0
