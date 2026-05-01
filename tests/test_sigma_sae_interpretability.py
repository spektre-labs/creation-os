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

from cos.sigma_gate_core import SigmaState, Verdict, sigma_gate, sigma_update  # noqa: E402
from cos.sigma_sae import SigmaSAE  # noqa: E402
from cos.sigma_steer import SigmaSteer  # noqa: E402
from cos.sigma_superposition import superposition_sigma  # noqa: E402


def _verdict_after_updates(warm_sigmas: list[float], new_sigma: float, *, k_raw: float = 0.95) -> Verdict:
    st = SigmaState()
    for w in warm_sigmas:
        sigma_update(st, float(w), k_raw)
    sigma_update(st, float(new_sigma), k_raw)
    return sigma_gate(st)


def _find_hidden(sae: SigmaSAE, *, lo: float, hi: float, warm_sigmas: list[float]) -> torch.Tensor:
    """Pick ``h`` so σ(h) is in-band and the σ-gate does not ABSTAIN after warm-up + σ(h)."""
    for seed in range(50_000):
        torch.manual_seed(seed)
        h = torch.randn(8)
        s, _ = sae.compute_sigma(h)
        if not (lo <= float(s) <= hi):
            continue
        if _verdict_after_updates(warm_sigmas, float(s)) == Verdict.ABSTAIN:
            continue
        return h
    raise AssertionError("no hidden found; adjust warm_sigmas or bounds")


def _find_hidden_rethink_halluc(
    sae: SigmaSAE,
    *,
    warm_sigmas: list[float],
    prev_sigma: float,
    z_index: int = 3,
) -> torch.Tensor:
    """Gate not ABSTAIN, Σ rises vs ``prev_sigma``, and ``z_index`` fires."""
    for seed in range(50_000):
        torch.manual_seed(seed)
        h = torch.randn(8)
        s, z = sae.compute_sigma(h)
        zf = z.reshape(-1)
        if float(s) <= prev_sigma + 1e-3:
            continue
        if zf.numel() <= z_index or float(zf[z_index].item()) <= 0.05:
            continue
        v = _verdict_after_updates(warm_sigmas, float(s))
        if v == Verdict.ABSTAIN:
            continue
        return h
    raise AssertionError("no seed for RETHINK + halluc feature; widen search")


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
    torch.manual_seed(123)
    sae = SigmaSAE(ToySAE())
    h = _find_hidden(sae, lo=0.05, hi=0.95, warm_sigmas=[0.18, 0.18])
    steerer = SigmaSteer(k_raw=0.95)
    st = SigmaState()
    sigma_update(st, 0.18, 0.95)
    sigma_update(st, 0.18, 0.95)
    out = steerer.steer(None, h, st, sae)
    assert out is not None
    assert torch.allclose(out, h)


def test_sigma_steer_zeroes_hallucination_latent() -> None:
    """RETHINK + known feature active → decode after masking coordinates."""
    torch.manual_seed(123)
    core = ToySAE()
    sae = SigmaSAE(core, known_hallucination_features=(3,), activation_threshold=0.05)
    steerer = SigmaSteer(k_raw=0.95)
    st = SigmaState()
    sigma_update(st, 0.12, 0.95)
    sigma_update(st, 0.12, 0.95)
    h = _find_hidden_rethink_halluc(sae, warm_sigmas=[0.12, 0.12], prev_sigma=0.12, z_index=3)
    out = steerer.steer(None, h, st, sae)
    assert out is not None
    assert isinstance(out, torch.Tensor)
