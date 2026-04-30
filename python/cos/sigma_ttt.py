# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
Test-time training (**TTT**) scaffold gated by ``sigma_gate_core``.

**Orientation:** Titans / fast-weight adapters / in-context learning loops — this file
keeps a **minimal** ``nn.Parameter`` delta on activations when ``Verdict.RETHINK`` fires,
then re-forwards once. It is not a faithful Hope/Titans reproduction.

``sigma_gate.h`` / kernel interrupt semantics remain authoritative; this is Python-lab only.
"""
from __future__ import annotations

from typing import Callable, Optional, Tuple

import torch
import torch.nn as nn

from .sigma_gate_core import SigmaState, Verdict, sigma_gate, sigma_update


class SigmaTTT(nn.Module):
    """Low-rank delta on activations; TTT step only on ``RETHINK``."""

    def __init__(self, *, d_model: int, lr: float = 1e-2) -> None:
        super().__init__()
        self.d_model = int(d_model)
        self.lr = float(lr)
        self.delta = nn.Parameter(torch.zeros(self.d_model, self.d_model))

    def compute_ttt_loss(self, x: torch.Tensor, output: torch.Tensor) -> torch.Tensor:
        """Drive ``delta`` from data: match cheap linear transform to current output."""
        adj = x @ self.delta
        return (output - adj).pow(2).mean()

    def update_last_layer(self, loss: torch.Tensor) -> None:
        grad = torch.autograd.grad(loss, self.delta, retain_graph=False, create_graph=False)[0]
        with torch.no_grad():
            self.delta -= self.lr * grad

    def forward(
        self,
        x: torch.Tensor,
        state: SigmaState,
        *,
        backbone: Callable[[torch.Tensor], torch.Tensor],
        probe_sigma: float,
        k_raw: float = 0.7,
    ) -> Tuple[Optional[torch.Tensor], Verdict]:
        """
        ``backbone`` maps activations → activations (caller supplies frozen trunk + head).
        ``probe_sigma`` is an external risk scalar in ``[0,1]`` (e.g. LSD) fed into the interrupt.
        """
        output = backbone(x)
        sigma_update(state, float(probe_sigma), float(k_raw))
        verdict = sigma_gate(state)
        if verdict == Verdict.RETHINK:
            loss = self.compute_ttt_loss(x, output.detach())
            self.update_last_layer(loss)
            output = backbone(x)
            sigma_update(state, float(probe_sigma), float(k_raw))
            verdict = sigma_gate(state)
        if verdict == Verdict.ABSTAIN:
            return None, verdict
        return output, verdict


__all__ = ["SigmaTTT"]
