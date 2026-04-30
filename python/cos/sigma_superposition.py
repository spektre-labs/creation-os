# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
**Superposition as σ:** density of simultaneously active SAE coordinates — a cheap proxy
for "many features at once" when a sparse dictionary is available.

This is a **harness prior**, not a substitute for full mechanistic causal scrubbing.
"""
from __future__ import annotations

from typing import Any, Tuple

from .sigma_sae import SigmaSAE

try:
    import torch
except ImportError:  # pragma: no cover
    torch = None  # type: ignore[misc, assignment]


def _require_torch() -> Any:
    if torch is None:
        raise ImportError("sigma_superposition requires PyTorch (`pip install torch`).")
    return torch


def superposition_sigma(
    hidden_states: Any,
    sae: Any,
    *,
    threshold: float = 0.1,
    ref_density: float = 0.1,
) -> Tuple[float, int]:
    """
    Encode ``hidden_states`` with ``sae`` (``SigmaSAE`` or ``encode``/``__call__`` object),
    count coordinates above ``threshold``, and map ``n_active / n_total`` vs ``ref_density``
    into ``[0, 1]``.
    """
    t = _require_torch()
    if not isinstance(hidden_states, t.Tensor):
        raise TypeError("hidden_states must be a torch.Tensor.")

    if isinstance(sae, SigmaSAE):
        features, _ = sae.encode_decode(hidden_states)
    elif hasattr(sae, "encode"):
        enc_out = sae.encode(hidden_states)
        features = enc_out[0] if isinstance(enc_out, tuple) else enc_out
    elif callable(sae):
        out = sae(hidden_states)
        if not isinstance(out, tuple) or len(out) < 1:
            raise TypeError("sae(hidden_states) must return at least (features, ...).")
        features = out[0]
    else:
        raise TypeError("sae must be SigmaSAE, have encode(), or be callable.")

    if not isinstance(features, t.Tensor):
        raise TypeError("encode path must yield a torch.Tensor of features.")

    z = features.detach().float().reshape(-1)
    n_active = int((z > float(threshold)).sum().item())
    n_total = max(int(z.numel()), 1)
    sparsity = float(n_active) / float(n_total)
    ref = max(float(ref_density), 1e-9)
    sigma = float(min(1.0, sparsity / ref))
    return sigma, n_active


__all__ = ["superposition_sigma"]
