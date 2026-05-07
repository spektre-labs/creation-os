# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.federated` (σ-screened toy federation; lab)."""
from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.federated import FederatedAggregator, FederatedClient  # noqa: E402


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
