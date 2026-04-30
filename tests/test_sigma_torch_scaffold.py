# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""``torch``-based σ scaffolds (skipped when PyTorch is not installed)."""
from __future__ import annotations

import sys
from pathlib import Path

import pytest

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

pytest.importorskip("torch")

import torch  # noqa: E402

from cos.sigma_deltanet import SigmaDeltaNet  # noqa: E402
from cos.sigma_gate_core import SigmaState, Verdict, sigma_update  # noqa: E402
from cos.sigma_ttt import SigmaTTT  # noqa: E402


def _warmed_state(sigma_01: float = 0.2, k_raw: float = 0.95) -> SigmaState:
    st = SigmaState()
    sigma_update(st, sigma_01, k_raw)
    sigma_update(st, sigma_01, k_raw)
    return st


def test_sigma_deltanet_fresh_rethink_full_path() -> None:
    net = SigmaDeltaNet(d_model=8, scale_sigma=100.0)
    st = SigmaState()
    x = torch.randn(1, 3, 8) * 0.05
    out, v = net(x, st)
    assert out is not None
    assert v == Verdict.RETHINK


def test_sigma_deltanet_warmed_small_linear_accept() -> None:
    net = SigmaDeltaNet(d_model=8, scale_sigma=500.0)
    st = _warmed_state()
    x = torch.randn(1, 2, 8) * 0.01
    out, v = net(x, st)
    assert out is not None
    assert v == Verdict.ACCEPT


def test_sigma_ttt_rethink_then_accept() -> None:
    ttt = SigmaTTT(d_model=4, lr=0.05)

    def backbone(z: torch.Tensor) -> torch.Tensor:
        return z * 2.0

    st = SigmaState()
    x = torch.randn(2, 4)
    out, v = ttt(x, st, backbone=backbone, probe_sigma=0.35, k_raw=0.92)
    assert out is not None
    assert v == Verdict.ACCEPT
