# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
from __future__ import annotations

import sys
from pathlib import Path

import pytest

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

torch = pytest.importorskip("torch", reason="torch required for sigma_icr tests")

from cos.sigma_icr import (  # noqa: E402
    compute_icr_scores,
    icr_ratios_from_hidden_stack,
    icr_sigma,
    icr_sigma_from_ratios,
)


def test_compute_icr_scores_matches_ratios_stack() -> None:
    t = torch.randn(1, 4, 16)
    hs = [t * 0.1, t * 0.2, t * 0.35]
    a = compute_icr_scores(hs)
    b = icr_ratios_from_hidden_stack(hs)
    assert len(a) == 2 and a == b


def test_icr_sigma_variance_squash() -> None:
    flat = [0.1, 0.1, 0.1, 0.1]
    assert icr_sigma(flat) == 0.0
    spread = [0.0, 0.5, 0.1, 0.9]
    s = icr_sigma(spread)
    assert 0.0 < s <= 1.0


def test_icr_sigma_from_ratios_softer_than_icr_sigma() -> None:
    xs = [0.0, 0.4, 0.2, 0.8]
    assert icr_sigma_from_ratios(xs) <= 1.0
