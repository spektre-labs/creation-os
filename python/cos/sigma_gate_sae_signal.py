# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
Cascade **L5 SAE** signal: reconstruction / dominance σ, Clarify-shaped abstain hints,
and top feature attribution for audit JSON.

Python-only — does **not** modify ``sigma_gate.h`` or C gate thresholds.
"""
from __future__ import annotations

from typing import Any, Dict, List, Sequence

try:
    import torch
except ImportError:  # pragma: no cover
    torch = None  # type: ignore[misc, assignment]

from .sigma_sae import SigmaSAE


def _require_torch() -> Any:
    if torch is None:
        raise ImportError("sigma_gate_sae_signal requires PyTorch (`pip install torch`).")
    return torch


def _top_attribution(
    features: Any,
    *,
    k: int = 8,
    feature_labels: Sequence[str] = (),
) -> List[Dict[str, Any]]:
    t = _require_torch()
    if not isinstance(features, t.Tensor):
        raise TypeError("features must be a torch.Tensor.")
    z = torch.relu(features.detach().float().reshape(-1))
    vals, idx = torch.sort(z, descending=True)
    out: List[Dict[str, Any]] = []
    limit = min(int(k), int(z.numel()))
    for i in range(limit):
        j = int(idx[i].item())
        mass = float(vals[i].item())
        if mass <= 0.0:
            break
        label = ""
        if 0 <= j < len(feature_labels):
            label = str(feature_labels[j])
        out.append({"index": j, "mass": mass, "label": label})
    return out


def compute_l5_sae_signal(
    sae: SigmaSAE,
    hidden_states: Any,
    *,
    clarify_tau: float = 0.62,
    feature_labels: Sequence[str] = (),
    top_k: int = 8,
) -> Dict[str, Any]:
    """
    Return a JSON-serializable L5 bundle for observability / CLI.

    ``recommend_abstain`` is true when ``clarify_score > clarify_tau`` (lab heuristic).
    """
    sigma_sae, features, meta = sae.aggregate_sigma_sae(hidden_states)
    clar = sae.clarify_scores(features)
    abstain = bool(float(clar["clarify_score"]) > float(clarify_tau))
    attrib = _top_attribution(features, k=int(top_k), feature_labels=feature_labels)
    return {
        "level": "L5_SAE",
        "sigma_sae": float(sigma_sae),
        "reconstruction_sigma": float(meta["reconstruction_sigma"]),
        "dominance_sigma": float(meta["dominance_sigma"]),
        "clarify_score": float(clar["clarify_score"]),
        "output_score": float(clar["output_score"]),
        "clarify_tau": float(clarify_tau),
        "recommend_abstain": abstain,
        "feature_attribution": attrib,
        "cb_sae_prune_plan": sae.plan_cb_sae_prune(features),
    }


__all__ = ["compute_l5_sae_signal", "_top_attribution"]
