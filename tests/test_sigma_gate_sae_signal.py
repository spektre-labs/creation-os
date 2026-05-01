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

from cos.sigma_gate_sae_signal import compute_l5_sae_signal  # noqa: E402
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


def test_l5_signal_keys() -> None:
    sae = SigmaSAE(ToySAE())
    h = torch.randn(8)
    labels = [f"f{i}" for i in range(16)]
    payload = compute_l5_sae_signal(sae, h, clarify_tau=0.99, feature_labels=labels)
    assert payload["level"] == "L5_SAE"
    assert "sigma_sae" in payload
    assert "recommend_abstain" in payload
    assert isinstance(payload["feature_attribution"], list)
    assert "cb_sae_prune_plan" in payload
