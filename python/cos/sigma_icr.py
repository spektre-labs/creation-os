# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
**ICR-style** cross-layer residual magnitude (lab scaffold).

**Orientation:** *Information Contribution to Residual Streams* framing (Zhang et al., ACL 2025,
arXiv:2507.16488) — this file is **not** a paper-faithful reimplementation; it is a cheap
scalar summary of ``||h_l - h_{l-1}|| / (||h_{l-1}||+eps)`` per layer for hallucination-oriented
risk fusion in harness code.

**Claim discipline:** bind any AUROC headline to ``docs/CLAIM_DISCIPLINE.md`` + archived JSON.
"""
from __future__ import annotations

from typing import Any, List, Sequence

import numpy as np

# Upper bound on empirical variance of per-layer ICR ratios (lab squash); tune locally.
_MAX_ICR_VAR_SQUASH = 0.25


def compute_icr_scores(hidden_states: Sequence[Any]) -> List[float]:
    """
    Per-layer residual magnitude ratios (ICR-style lab signal).

    Alias of :func:`icr_ratios_from_hidden_stack` — matches the “ICR score list” naming
    used in harness docs (Zhang et al., ACL 2025, arXiv:2507.16488 as **orientation**).
    """
    return icr_ratios_from_hidden_stack(hidden_states)


def icr_ratios_from_hidden_stack(hidden_states: Sequence[Any]) -> List[float]:
    """
    ``hidden_states``: tuple/list of length ``L+1`` (embedding + blocks), each ``[1, T, D]`` tensor.
    Returns length ``L`` list of mean-norm residual ratios per layer transition.
    """
    import torch

    if not hidden_states or len(hidden_states) < 2:
        return []
    ratios: List[float] = []
    for li in range(1, len(hidden_states)):
        h_prev = hidden_states[li - 1]
        h_cur = hidden_states[li]
        if not isinstance(h_prev, torch.Tensor) or not isinstance(h_cur, torch.Tensor):
            continue
        if h_prev.ndim < 3 or h_cur.ndim < 3:
            continue
        delta = (h_cur - h_prev).detach().float()
        dnorm = float(torch.linalg.vector_norm(delta, dim=-1).mean().item())
        snorm = float(torch.linalg.vector_norm(h_prev.detach().float(), dim=-1).mean().item())
        ratios.append(float(dnorm / (snorm + 1e-8)))
    return ratios


def icr_sigma(icr_scores: Sequence[float]) -> float:
    """
    Map per-layer ICR ratios to a single ``[0, 1]`` risk via **variance squash** (divider 0.01).

    Higher cross-layer variance → higher σ (lab default for fusion / dashboards).
    """
    if not icr_scores:
        return 0.5
    v = float(np.var(np.asarray(list(icr_scores), dtype=np.float64)))
    return float(min(v / 0.01, 1.0))


def icr_sigma_from_ratios(ratios: Sequence[float]) -> float:
    """Map ICR ratio list to ``[0,1]`` risk: higher variance → higher σ (softer squash)."""
    if not ratios:
        return 0.5
    v = float(np.var(np.asarray(ratios, dtype=np.float64)))
    return float(np.clip(v / max(_MAX_ICR_VAR_SQUASH, 1e-9), 0.0, 1.0))


def icr_sigma_from_model_forward(
    model: Any,
    tokenizer: Any,
    text: str,
    *,
    max_length: int = 512,
) -> tuple[float, dict[str, Any]]:
    """
    Single forward on ``text``; returns ``(sigma_icr, details)``.
    """
    import torch

    enc = tokenizer(
        (text or "").strip(),
        return_tensors="pt",
        truncation=True,
        max_length=int(max_length),
        add_special_tokens=True,
    )
    dev = next(model.parameters()).device
    enc = {k: v.to(dev) for k, v in enc.items()}
    with torch.no_grad():
        out = model(**enc, output_hidden_states=True)
    hs = out.hidden_states
    ratios = icr_ratios_from_hidden_stack(hs)
    sig = icr_sigma_from_ratios(ratios)
    return sig, {
        "icr_ratios": ratios,
        "n_layers_used": len(ratios),
        "variance": float(np.var(ratios)) if ratios else 0.0,
        "sigma_icr": sig,
    }


__all__ = [
    "compute_icr_scores",
    "icr_ratios_from_hidden_stack",
    "icr_sigma",
    "icr_sigma_from_ratios",
    "icr_sigma_from_model_forward",
]
