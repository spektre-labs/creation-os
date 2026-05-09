# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.federated` (σ-screened toy federation; lab)."""
from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.integrations.federated import (  # noqa: E402
    FederatedAggregator,
    FederatedClient,
    FederatedNode,
    FederatedSigma,
)


class _FixedGate:
    def __init__(self, sigma: float) -> None:
        self.sigma = float(sigma)

    def score(self, _prompt: str, _response: str):
        return self.sigma, "ACCEPT"


class _RoundSplitGate:
    """High σ for prompts starting with ``bad``, low σ otherwise."""

    def score(self, prompt: str, _response: str):
        if str(prompt).startswith("bad"):
            return 0.85, "ABSTAIN"
        return 0.05, "ACCEPT"


def test_client_train_local() -> None:
    gate = _FixedGate(0.22)
    client = FederatedClient("c1", gate=gate)
    batch = [("p1", "r1"), ("p2", "r2")]
    up = client.train_local(batch)
    assert up["client_id"] == "c1"
    assert up["n_samples"] == 2
    assert up["sigma_avg"] == 0.22
    assert "update_from_c1" in str(up["update"])
    assert len(client.local_sigma_history) == 1


def test_client_unlearn() -> None:
    gate = _FixedGate(0.1)
    client = FederatedClient("c2", gate=gate)
    a, b, c = ("p", "a"), ("p", "b"), ("p", "c")
    client.train_local([a, b, c])
    rep = client.unlearn([b])
    assert rep["removed"] == 1
    assert rep["remaining"] == 2
    assert b not in client.local_data


def test_aggregator_accepts_low_sigma() -> None:
    agg = FederatedAggregator(max_sigma=0.5)
    r = agg.aggregate(
        [{"client_id": "a", "n_samples": 1, "sigma_avg": 0.1, "update": "u"}],
    )
    assert r["accepted"] == 1
    assert r["rejected"] == 0
    assert r["rejected_clients"] == []


def test_aggregator_rejects_high_sigma() -> None:
    agg = FederatedAggregator(max_sigma=0.5)
    r = agg.aggregate(
        [{"client_id": "b", "n_samples": 1, "sigma_avg": 0.91, "update": "u"}],
    )
    assert r["accepted"] == 0
    assert r["rejected"] == 1
    assert "b" in r["rejected_clients"]


def test_run_round_full_cycle() -> None:
    agg = FederatedAggregator(max_sigma=0.5)
    c1 = FederatedClient("n1", gate=_FixedGate(0.1))
    c2 = FederatedClient("n2", gate=_FixedGate(0.2))
    agg.register_client(c1)
    agg.register_client(c2)
    out = agg.run_round(
        {
            "n1": [("x", "y")],
            "n2": [("x", "z")],
        },
    )
    assert out["total_updates"] == 2
    assert out["accepted"] == 2
    assert out["round"] == 1


def test_global_sigma_tracking() -> None:
    agg = FederatedAggregator(max_sigma=0.5)
    agg.aggregate([{"client_id": "a", "sigma_avg": 0.2, "n_samples": 1, "update": ""}])
    agg.aggregate([{"client_id": "b", "sigma_avg": 0.4, "n_samples": 1, "update": ""}])
    g = agg.global_sigma()
    assert g == 0.3


def test_multiple_rounds_improve() -> None:
    """Later round accepts after unlearning / better batch (low σ vs high σ)."""
    agg = FederatedAggregator(max_sigma=0.5)
    gate = _RoundSplitGate()
    client = FederatedClient("learn", gate=gate)
    agg.register_client(client)
    r1 = agg.run_round({"learn": [("bad-late", "x"), ("bad-late", "y")]})
    assert r1["accepted"] == 0 and r1["rejected"] == 1
    client.unlearn(tuple(client.local_data))
    r2 = agg.run_round({"learn": [("good", "x"), ("good", "y")]})
    assert r2["accepted"] == 1 and r2["rejected"] == 0
    assert r2["round"] == 2


# --- FederatedNode / FederatedSigma (scalar-only reports) ---


def test_add_node() -> None:
    fed = FederatedSigma()
    n = fed.add_node("edge-1", gate=_FixedGate(0.2))
    assert n.node_id == "edge-1"
    assert "edge-1" in fed.nodes


def test_measure_stays_local() -> None:
    fed = FederatedSigma()
    n1 = fed.add_node("a", gate=_FixedGate(0.15))
    fed.add_node("b", gate=_FixedGate(0.2))
    n1.measure("local-prompt", "local-response")
    assert len(n1.local_sigma_history) == 1
    assert len(fed.nodes["b"].local_sigma_history) == 0


def test_report_only_sigma() -> None:
    n = FederatedNode("solo", gate=_FixedGate(0.33))
    n.measure("p", "r")
    rep = n.report()
    assert set(rep.keys()) == {"node", "avg_σ", "n"}
    assert rep["n"] == 1


def test_aggregate_weighted() -> None:
    fed = FederatedSigma()
    n_lo = fed.add_node("lo", gate=_FixedGate(0.2))
    n_hi = fed.add_node("hi", gate=_FixedGate(0.4))
    for _ in range(2):
        n_lo.measure("p", "r")
    for _ in range(4):
        n_hi.measure("p", "r")
    out = fed.aggregate()
    expect = (0.2 * 2 + 0.4 * 4) / 6.0
    assert abs(out["global_σ"] - round(expect, 4)) < 1e-6
    assert out["total_samples"] == 6


def test_detect_outlier_nodes() -> None:
    fed = FederatedSigma()
    for cid, sigma in [("h1", 0.1), ("h2", 0.1), ("h3", 0.1), ("bad", 0.95)]:
        n = fed.add_node(cid, gate=_FixedGate(sigma))
        for _ in range(3):
            n.measure("x", "y")
    fed.aggregate()
    det = fed.detect_outlier_nodes(deviation=0.3)
    assert det["n_outliers"] >= 1
    assert any(o["node"] == "bad" for o in det["outliers"])


def test_byzantine_robust() -> None:
    fed = FederatedSigma()
    for i in range(4):
        n = fed.add_node(f"h{i}", gate=_FixedGate(0.1))
        for _ in range(5):
            n.measure("a", "b")
    nb = fed.add_node("byz", gate=_FixedGate(0.99))
    for _ in range(5):
        nb.measure("a", "b")
    plain = fed.aggregate()
    fed2 = FederatedSigma()
    for i in range(4):
        n = fed2.add_node(f"h{i}", gate=_FixedGate(0.1))
        for _ in range(5):
            n.measure("a", "b")
    nb2 = fed2.add_node("byz", gate=_FixedGate(0.99))
    for _ in range(5):
        nb2.measure("a", "b")
    trimmed = fed2.byzantine_robust_aggregate()
    assert trimmed["global_σ"] < plain["global_σ"]


def test_global_threshold_distributed() -> None:
    fed = FederatedSigma()
    n1 = fed.add_node("x", gate=_FixedGate(0.2))
    n1.measure("a", "b")
    fed.aggregate()
    thr = fed.global_threshold
    assert all(abs(node.local_threshold - thr) < 1e-9 for node in fed.nodes.values())
