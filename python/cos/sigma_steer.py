# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
σ-governed **feature steering**: when the interrupt says ``RETHINK`` and the SAE flags
known risky coordinates, zero those dictionary entries and decode back to activations.

**Lab scaffold:** ``model`` is reserved for future hooked forwards; steering is purely
``hidden_states → hidden_states`` in activation space today.
"""
from __future__ import annotations

from typing import Any, Optional, Tuple

from .sigma_gate_core import SigmaState, Verdict, sigma_gate, sigma_update
from .sigma_sae import SigmaSAE

try:
    import torch
except ImportError:  # pragma: no cover
    torch = None  # type: ignore[misc, assignment]


def _require_torch() -> Any:
    if torch is None:
        raise ImportError("sigma_steer requires PyTorch (`pip install torch`).")
    return torch


class SigmaSteer:
    """Gate-first steering on SAE dictionary activations."""

    def __init__(self, *, k_raw: float = 0.92) -> None:
        self.k_raw = float(k_raw)

    def steer(
        self,
        model: Any,
        hidden_states: Any,
        sigma_state: SigmaState,
        sae: SigmaSAE,
        *,
        k_raw: Optional[float] = None,
    ) -> Optional[Any]:
        """
        Update ``sigma_state`` from SAE reconstruction σ; on ``RETHINK`` with known
        hallucination features active, zero those indices and ``decode``; return
        steered tensor or ``None`` on ``ABSTAIN``.
        """
        _ = model
        t = _require_torch()
        if not isinstance(hidden_states, t.Tensor):
            raise TypeError("hidden_states must be a torch.Tensor.")
        if not isinstance(sae, SigmaSAE):
            raise TypeError("sae must be a SigmaSAE instance.")

        kr = float(self.k_raw if k_raw is None else k_raw)
        kr = max(0.0, min(1.0, kr))

        sigma, features = sae.compute_sigma(hidden_states)
        sigma = max(0.0, min(1.0, float(sigma)))
        sigma_update(sigma_state, sigma, kr)
        verdict = sigma_gate(sigma_state)

        if verdict == Verdict.ACCEPT:
            return hidden_states

        if verdict == Verdict.RETHINK:
            is_halluc, halluc_features = sae.detect_hallucination_features(features)
            if not is_halluc:
                return hidden_states

            z = features.clone() if isinstance(features, t.Tensor) else features
            if not isinstance(z, t.Tensor):
                return hidden_states
            for idx in halluc_features:
                if z.dim() == 1:
                    z[int(idx)] = 0.0
                else:
                    z.reshape(-1)[int(idx)] = 0.0
            return sae.decode(z)

        return None


__all__ = ["SigmaSteer"]
