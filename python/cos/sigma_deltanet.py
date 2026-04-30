# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
σ-governed **linear recurrent attention** scaffold (DeltaNet-style hybrid).

**Orientation:** gated delta / linear recurrent layers (e.g. DeltaNet family) and
hybrid ``3:1`` linear-to-full stacks appear in recent linear-attention LM designs
(Qwen3-Next, Kimi K2 Linear, etc.) — this module is a **lab scaffold**, not a
weight-compatible drop-in for those checkpoints.

**Policy:** run a cheap linear map, map output to a scalar risk, feed ``sigma_gate_core``;
``ACCEPT`` keeps the linear output, ``RETHINK`` falls back to a dense attention path,
``ABSTAIN`` returns ``None``.

See ``docs/CLAIM_DISCIPLINE.md`` — no throughput or downstream headline without harness JSON.
"""
from __future__ import annotations

from typing import Any, Callable, Optional, Tuple

import torch
import torch.nn as nn

from .sigma_gate_core import SigmaState, Verdict, sigma_gate, sigma_update


class SigmaDeltaNet(nn.Module):
    """
    σ-hybrid: linear path first, dense fallback on ``RETHINK``.

    Default layers are tiny learned projections so the module is unit-testable without a
    full transformer; inject ``delta_fn`` / ``full_fn`` for research kernels.
    """

    def __init__(
        self,
        *,
        d_model: int = 64,
        scale_sigma: float = 8.0,
        delta_fn: Optional[Callable[[torch.Tensor], torch.Tensor]] = None,
        full_fn: Optional[Callable[[torch.Tensor], torch.Tensor]] = None,
    ) -> None:
        super().__init__()
        self.d_model = int(d_model)
        self._scale = float(scale_sigma)
        self._delta_fn = delta_fn
        self._full_fn = full_fn
        self._w_delta = nn.Parameter(torch.randn(self.d_model, self.d_model) * 0.02)
        self._w_full_q = nn.Parameter(torch.randn(self.d_model, self.d_model) * 0.02)
        self._w_full_k = nn.Parameter(torch.randn(self.d_model, self.d_model) * 0.02)
        self._w_full_v = nn.Parameter(torch.randn(self.d_model, self.d_model) * 0.02)

    def delta_attention(self, x: torch.Tensor) -> torch.Tensor:
        if self._delta_fn is not None:
            return self._delta_fn(x)
        return torch.tanh(x @ self._w_delta)

    def full_attention(self, x: torch.Tensor) -> torch.Tensor:
        if self._full_fn is not None:
            return self._full_fn(x)
        q = x @ self._w_full_q
        k = x @ self._w_full_k
        v = x @ self._w_full_v
        att = torch.softmax(q @ k.transpose(-2, -1) / (q.shape[-1] ** 0.5), dim=-1)
        return att @ v

    def _scalar_risk(self, output: torch.Tensor) -> float:
        n = float(torch.linalg.vector_norm(output.detach().float()).item())
        return float(max(0.0, min(1.0, n / max(self._scale, 1e-6))))

    def forward(self, x: torch.Tensor, state: SigmaState) -> Tuple[Optional[torch.Tensor], Verdict]:
        out_lin = self.delta_attention(x)
        sp = self._scalar_risk(out_lin)
        k_raw = max(0.0, min(1.0, 1.0 - sp))
        sigma_update(state, sp, k_raw)
        verdict = sigma_gate(state)
        if verdict == Verdict.ACCEPT:
            return out_lin, verdict
        if verdict == Verdict.RETHINK:
            return self.full_attention(x), verdict
        return None, verdict


__all__ = ["SigmaDeltaNet"]
