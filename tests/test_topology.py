# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.topology import SigmaTopology  # noqa: E402


def test_persistence_diagram_from_trace() -> None:
    T = SigmaTopology()
    trace = [0.9, 0.9, 0.1, 0.1, 0.9, 0.9]
    d = T.persistence_diagram(trace)
    assert d["n_features"] >= 1
    assert all("birth" in p and "death" in p for p in d["pairs"])


def test_persistent_vs_noise_features() -> None:
    T = SigmaTopology()
    # Long low segment → persistent; single-step dip → short lifetime → noise bucket
    trace = [0.8] * 30 + [0.1] * 12 + [0.8] * 8 + [0.1] + [0.8] * 49
    assert len(trace) == 100
    d = T.persistence_diagram(trace)
    assert d["persistent_features"] + d["noise_features"] == d["n_features"]
    assert d["persistent_features"] >= 1
    assert d["noise_features"] >= 1


def test_betti_numbers_at_thresholds() -> None:
    T = SigmaTopology()
    trace = [0.05, 0.05, 0.5, 0.5]
    b = T.betti_numbers(trace, thresholds=[0.2, 0.6])
    assert len(b) == 2
    assert b[0]["beta0"] >= 1
    assert "β0" in b[0]


def test_landscape_dominant_feature() -> None:
    T = SigmaTopology()
    trace = [0.8, 0.8, 0.1, 0.1, 0.1, 0.8, 0.8]
    L = T.landscape(trace, k=3)
    diag = T.persistence_diagram(trace)
    assert diag["pairs"]
    want = max(int(p["lifetime"]) for p in diag["pairs"])
    assert L["dominant_feature_lifetime"] == want
    assert len(L["landscapes"]) <= 3


def test_curvature_smooth_trace() -> None:
    T = SigmaTopology()
    trace = [0.5] * 12
    c = T.sigma_manifold_curvature(trace, window=5)
    assert c["smooth"] is True
    assert c["avg_curvature"] == 0.0


def test_curvature_turbulent_trace() -> None:
    T = SigmaTopology()
    trace = [0.0 if i % 2 == 0 else 1.0 for i in range(20)]
    c = T.sigma_manifold_curvature(trace, window=5)
    assert c["smooth"] is False
    assert c["avg_curvature"] > 0.05


def test_wasserstein_distance_same_is_zero() -> None:
    T = SigmaTopology()
    tr = [0.7, 0.7, 0.2, 0.2, 0.7, 0.7, 0.7]
    assert T.wasserstein_distance(tr, list(tr)) == 0.0
