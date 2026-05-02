# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for ``cos.sigma_conflict`` (v166 multi-node resolution)."""
from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.sigma_conflict import (  # noqa: E402
    SigmaConflict,
    SigmaConflictResolver,
    normalize_node_row,
)


def test_triangulation_two_thirds_paris_low_sigma_wins_railo() -> None:
    rows = [
        {"node_id": "railo", "response": "Paris", "sigma": 0.05, "verdict": "ACCEPT"},
        {"node_id": "hanna", "response": "London", "sigma": 0.34, "verdict": "ACCEPT"},
        {"node_id": "niko", "response": "Paris", "sigma": 0.08, "verdict": "ACCEPT"},
    ]
    sc = SigmaConflict(None, strategy="triangulation")
    out = sc.resolve(rows)
    assert out is not None
    assert out["node_id"] == "railo"
    assert out["response"] == "Paris"


def test_sigma_wins_when_all_disagree_picks_lowest_sigma() -> None:
    rows = [
        {"node_id": "a", "response": "aaa", "sigma": 0.91},
        {"node_id": "b", "response": "bbb", "sigma": 0.08},
        {"node_id": "c", "response": "ccc", "sigma": 0.55},
    ]
    out = SigmaConflict(None, strategy="sigma_wins").resolve(rows)
    assert out is not None
    assert out["node_id"] == "b"
    assert float(out["sigma"]) == 0.08


def test_proconductor_returns_pending_bundle() -> None:
    rows = [
        {"node_id": "railo", "response": "yes", "sigma": 0.04, "verdict": "ACCEPT"},
        {"node_id": "hanna", "response": "no", "sigma": 0.06, "verdict": "ACCEPT"},
    ]
    out = SigmaConflict(None, strategy="proconductor").resolve(rows)
    assert out is not None
    assert out["node_id"] == "proconductor"
    assert out["verdict"] == "PENDING"
    assert out.get("requires_human") is True
    assert isinstance(out.get("options"), list)
    assert len(out["options"]) == 2


def test_accept_vs_abstain_classified() -> None:
    rows = [
        {"node_id": "a", "response": "same", "sigma": 0.1, "verdict": "ACCEPT"},
        {"node_id": "b", "response": "same", "sigma": 0.12, "verdict": "ABSTAIN"},
    ]
    sc = SigmaConflict(None, strategy="sigma_wins")
    norm = [normalize_node_row(r) for r in rows]
    assert sc.classify_conflict(norm) == "accept_vs_abstain"


def test_resolver_legacy_node_result_keys() -> None:
    stub = [
        {"node": "n1", "result": "one", "sigma": 0.5},
        {"node": "n2", "result": "two", "sigma": 0.1},
    ]
    res = SigmaConflictResolver(strategy="sigma_wins").resolve(stub)
    assert res["winner"] == "n2"
    assert res["resolution"]["node_id"] == "n2"
