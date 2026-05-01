# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
from __future__ import annotations

import sys
from pathlib import Path

import pytest

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

pytest.importorskip("torch")

import torch  # noqa: E402
import torch.nn as nn  # noqa: E402

from cos.sigma_sae import SigmaSAE  # noqa: E402


class ToySAE(nn.Module):
    def __init__(self, d_in: int = 8, d_dict: int = 16) -> None:
        super().__init__()
        self.enc = nn.Linear(d_in, d_dict, bias=True)
        self.dec = nn.Linear(d_dict, d_in, bias=True)

    def encode(self, h: torch.Tensor) -> torch.Tensor:
        return torch.relu(self.enc(h))

    def decode(self, z: torch.Tensor) -> torch.Tensor:
        return self.dec(z)


def test_aggregate_sigma_sae_bounded() -> None:
    sae = SigmaSAE(ToySAE())
    h = torch.randn(8)
    agg, feat, meta = sae.aggregate_sigma_sae(h)
    assert 0.0 <= agg <= 1.0
    assert "reconstruction_sigma" in meta
    assert feat.shape[-1] == 16


def test_clarify_scores_sum_to_one() -> None:
    sae = SigmaSAE(ToySAE())
    h = torch.randn(8)
    _, feat, _ = sae.aggregate_sigma_sae(h)
    c = sae.clarify_scores(feat)
    assert abs(c["clarify_score"] + c["output_score"] - 1.0) < 1e-5


def test_cb_sae_plan_partitions_indices() -> None:
    sae = SigmaSAE(ToySAE())
    h = torch.randn(8)
    _, feat, _ = sae.aggregate_sigma_sae(h)
    plan = sae.plan_cb_sae_prune(feat, tau_interp=0.0, tau_steer=0.0)
    n = feat.numel() if feat.dim() == 1 else int(feat.reshape(-1).numel())
    assert len(plan["prune_indices"]) + len(plan["keep_indices"]) == n
