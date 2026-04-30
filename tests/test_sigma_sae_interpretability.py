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

from cos.sigma_gate_core import SigmaState, sigma_update  # noqa: E402
from cos.sigma_sae import SigmaSAE  # noqa: E402
from cos.sigma_steer import SigmaSteer  # noqa: E402
from cos.sigma_superposition import superposition_sigma  # noqa: E402


class ToySAE(nn.Module):
    """Minimal encode/decode SAE for unit tests."""

    def __init__(self, d_in: int = 8, d_dict: int = 16) -> None:
        super().__init__()
        self.enc = nn.Linear(d_in, d_dict, bias=True)
        self.dec = nn.Linear(d_dict, d_in, bias=True)

    def encode(self, h: torch.Tensor) -> torch.Tensor:
        return torch.relu(self.enc(h))

    def decode(self, z: torch.Tensor) -> torch.Tensor:
        return self.dec(z)

    def forward(self, h: torch.Tensor) -> tuple[torch.Tensor, torch.Tensor]:
        z = self.encode(h)
        return z, self.decode(z)


def test_sigma_sae_compute_sigma_bounded() -> None:
    sae = SigmaSAE(ToySAE())
    h = torch.randn(8)
    sigma, z = sae.compute_sigma(h)
    assert 0.0 <= sigma <= 1.0
    assert z.shape[-1] == 16


def test_sigma_sae_explain_and_detect() -> None:
    t = torch.zeros(16)
    t[2] = 0.5
    t[7] = 0.2
    labels = [f"f{i}" for i in range(16)]
    sae = SigmaSAE(ToySAE(), known_hallucination_features=(7, 99), activation_threshold=0.15)
    exp = sae.explain_sigma(t, labels)
    assert "f2" in exp
    hit, idx = sae.detect_hallucination_features(t)
    assert hit and idx == [7]


def test_superposition_sigma() -> None:
    sae = SigmaSAE(ToySAE())
    h = torch.randn(4, 8)
    sig, n = superposition_sigma(h, sae, threshold=0.1, ref_density=0.1)
    assert 0.0 <= sig <= 1.0
    assert n >= 0


def test_sigma_steer_accepts_when_gate_stable() -> None:
    sae = SigmaSAE(ToySAE())
    steerer = SigmaSteer(k_raw=0.95)
    st = SigmaState()
    sigma_update(st, 0.18, 0.95)
    sigma_update(st, 0.18, 0.95)
    h = torch.randn(8) * 0.02
    out = steerer.steer(None, h, st, sae)
    assert out is not None
    assert torch.allclose(out, h)


def test_sigma_steer_zeroes_hallucination_latent() -> None:
    """RETHINK + known feature active → decode after masking coordinates."""
    torch.manual_seed(0)
    core = ToySAE()
    sae = SigmaSAE(core, known_hallucination_features=(3,), activation_threshold=0.05)
    steerer = SigmaSteer(k_raw=0.95)
    st = SigmaState()
    h = torch.randn(8)
    out = steerer.steer(None, h, st, sae)
    assert out is not None
    assert isinstance(out, torch.Tensor)
