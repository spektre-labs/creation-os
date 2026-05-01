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
from cos.sigma_steering import SigmaSteering  # noqa: E402


class ToySAE(nn.Module):
    def __init__(self, d_in: int = 8, d_dict: int = 16) -> None:
        super().__init__()
        self.enc = nn.Linear(d_in, d_dict, bias=True)
        self.dec = nn.Linear(d_dict, d_in, bias=True)

    def encode(self, h: torch.Tensor) -> torch.Tensor:
        return torch.relu(self.enc(h))

    def decode(self, z: torch.Tensor) -> torch.Tensor:
        return self.dec(z)


def test_inference_direction_steering_shape() -> None:
    st = SigmaSteering()
    h = torch.randn(8)
    uf = torch.randn(8)
    uh = torch.randn(8)
    out = st.inference_direction_steering(
        h,
        faithful_direction=uf,
        hallucinatory_direction=uh,
        scale_faithful=0.1,
        scale_halluc_down=0.2,
    )
    assert out.shape == h.shape


def test_correlate_sigma_with_activations_perfect() -> None:
    st = SigmaSteering()
    sig = [0.1, 0.2, 0.3, 0.4]
    rows = [[float(i), 0.0] for i in range(4)]
    for i, s in enumerate(sig):
        rows[i][0] = s
    corr = st.correlate_sigma_with_activations(sig, rows)
    assert corr[0]["pearson_r"] > 0.99


def test_permanent_patch_requires_hits_and_drop() -> None:
    st = SigmaSteering()
    assert st.suggest_permanent_patch(3, sigma_drop_if_ablated=0.9) is None
    st.record_hallucination_hit(3)
    st.record_hallucination_hit(3)
    assert st.suggest_permanent_patch(3, min_hits=3, sigma_drop_if_ablated=0.9, alpha_crit=0.2) is None
    st.record_hallucination_hit(3)
    patch = st.suggest_permanent_patch(3, min_hits=3, sigma_drop_if_ablated=0.25, alpha_crit=0.15)
    assert patch is not None
    assert patch["feature_idx"] == 3


def test_steer_with_sigma_sae_runs() -> None:
    sae = SigmaSAE(ToySAE())
    st = SigmaSteering()
    h = torch.randn(8) * 0.1
    out = st.steer_with_sigma_sae(sae, h, faithful_direction=torch.ones(8), scale_faithful=0.01)
    assert "sigma_sae" in out
    assert 0.0 <= float(out["sigma_sae"]) <= 1.0
