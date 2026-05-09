# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.theory.geometry import SigmaManifold  # noqa: E402


def test_fisher_distance_zero_same_point() -> None:
    M = SigmaManifold()
    assert M.fisher_distance(0.35, 0.35) == 0.0


def test_fisher_distance_increases_with_gap() -> None:
    M = SigmaManifold()
    d_small = M.fisher_distance(0.4, 0.45)
    d_large = M.fisher_distance(0.1, 0.9)
    assert d_small < d_large


def test_kl_divergence_zero_same() -> None:
    M = SigmaManifold()
    assert M.kl_divergence(0.25, 0.25) == 0.0


def test_kl_divergence_asymmetric() -> None:
    M = SigmaManifold()
    a = M.kl_divergence(0.2, 0.85)
    b = M.kl_divergence(0.85, 0.2)
    assert a != b


def test_natural_gradient_step() -> None:
    M = SigmaManifold()
    out = M.natural_gradient(0.82, sigma_target=0.0, learning_rate=0.5)
    assert out["σ_after"] < out["σ_before"]


def test_geodesic_path_endpoints() -> None:
    M = SigmaManifold()
    g = M.geodesic(0.88, 0.12, n_steps=12)
    assert abs(g["path"][0] - 0.88) < 0.02
    assert abs(g["path"][-1] - 0.12) < 0.02
    assert g["is_recovery"] is True


def test_curvature_high_at_boundary() -> None:
    M = SigmaManifold()
    hi = M.curvature_at(0.06)["curvature"]
    lo = M.curvature_at(0.5)["curvature"]
    assert hi > lo


def test_gradient_flow_efficiency() -> None:
    M = SigmaManifold()
    geo = M.geodesic(0.85, 0.15, n_steps=8)
    good_trace = [geo["path"][0], geo["path"][4], geo["path"][-1]]
    bad_trace = [0.85, 0.2, 0.82, 0.18]
    g_flow = M.sigma_gradient_flow(good_trace)
    b_flow = M.sigma_gradient_flow(bad_trace)
    assert g_flow["efficient"] is True
    assert b_flow["efficient"] is False
