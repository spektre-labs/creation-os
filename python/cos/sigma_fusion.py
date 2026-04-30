# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
Fuse multiple hallucination-oriented **σ** channels (LSD, HIDE, ICR, entropy, spectral, SAE).

**Harness-only:** default weights are a reproducible prior, not tuned on a leaderboard.
See ``docs/CLAIM_DISCIPLINE.md`` for headline AUROC hygiene.
"""
from __future__ import annotations

import math
from typing import Any, Dict, Mapping, MutableMapping, Optional, Tuple

import numpy as np

from .sigma_gate_core import SigmaState, Verdict, sigma_gate, sigma_update

# Keys must match ``run_icr_eval`` / callers.
DEFAULT_WEIGHTS: Dict[str, float] = {
    "lsd": 0.35,
    "hide": 0.25,
    "icr": 0.25,
    "entropy": 0.15,
}


def _finite(x: float) -> bool:
    return math.isfinite(x)


def fused_sigma(
    signals: Mapping[str, float],
    weights: Optional[Mapping[str, float]] = None,
    *,
    mode: str = "weighted_mean",
) -> float:
    """
    Combine per-signal risks in ``[0, 1]``.

    ``mode``:
      * ``weighted_mean`` — weight-normalized mean over keys present in both dicts.
      * ``max`` — max of present finite signals.
    """
    w_src: Mapping[str, float] = weights if weights is not None else DEFAULT_WEIGHTS
    mode_l = (mode or "weighted_mean").strip().lower()
    keys = [k for k in signals if _finite(float(signals[k]))]
    if not keys:
        return 0.5
    if mode_l == "max":
        return float(max(float(signals[k]) for k in keys))
    acc = 0.0
    wsum = 0.0
    for k in keys:
        w = float(w_src.get(k, 0.0))
        if w <= 0.0:
            continue
        acc += w * float(signals[k])
        wsum += w
    if wsum <= 0.0:
        return float(sum(float(signals[k]) for k in keys) / len(keys))
    return float(max(0.0, min(1.0, acc / wsum)))


def normalize_weights(weights: Mapping[str, float]) -> Dict[str, float]:
    """Return a copy of ``weights`` renormalized to sum to 1 (only positive entries)."""
    out: Dict[str, float] = {k: float(v) for k, v in weights.items() if float(v) > 0.0}
    s = sum(out.values())
    if s <= 0.0:
        return dict(DEFAULT_WEIGHTS)
    return {k: v / s for k, v in out.items()}


def merge_weight_dict(base: Mapping[str, float], override: Optional[Mapping[str, float]]) -> Dict[str, float]:
    """``base`` updated with ``override`` keys (JSON harness convenience)."""
    m: MutableMapping[str, float] = dict(base)
    if override:
        for k, v in override.items():
            m[str(k)] = float(v)
    return normalize_weights(m)


def entropy_sigma_from_logprobs(log_probs_1d: Any) -> float:
    """
    Normalized Shannon entropy of a **1-D** log-probability vector → ``[0,1]`` risk proxy.

    Higher value ⇒ more uncertain / higher ``σ`` in the same convention as ``SigmaPrecheck``.
    """
    lp = np.asarray(log_probs_1d, dtype=np.float64).ravel()
    if lp.size == 0:
        return 0.5
    p = np.exp(lp - np.max(lp))
    p = p / (np.sum(p) + 1e-12)
    ent = float(-np.sum(p * np.log(p + 1e-12)))
    vmax = float(np.log(max(int(p.size), 2)))
    return float(max(0.0, min(1.0, ent / max(vmax, 1e-9))))


class SigmaFusion:
    """
    **Cascade** over precomputed scalar risks (entropy → HIDE → ICR → mean →
    LSD/spectral/**SAE last** → cognitive).

    Each ``scores`` value is a hallucination-oriented risk in ``[0,1]`` (higher = riskier),
    matching ``run_icr_eval`` / ``SigmaPrecheck`` conventions. The ``sae`` slot is intended
    for heavier sparse-autoencoder probes and is pooled **after** ``spectral``.

    Early **ACCEPT** exits keep the harness cheap; the final stage mirrors ``sigma_gate_core``
    on a pooled scalar for a discrete ``Verdict``.
    """

    def __init__(
        self,
        *,
        tau_fast: float = 0.42,
        tau_verify: float = 0.58,
    ) -> None:
        self.tau_fast = float(tau_fast)
        self.tau_verify = float(tau_verify)

    def cascade(self, scores: Mapping[str, Optional[float]]) -> Tuple[Verdict, float, str]:
        """
        Returns ``(verdict, pooled_scalar, route_note)``.

        ``scores`` keys may include: ``entropy``, ``hide``, ``icr``, ``lsd``, ``spectral``, ``sae``.
        Missing keys are skipped for that tier.
        """
        ent = scores.get("entropy")
        if ent is not None and _finite(float(ent)) and float(ent) < self.tau_fast:
            return Verdict.ACCEPT, float(ent), "entropy_fast"

        hid = scores.get("hide")
        if hid is not None and _finite(float(hid)) and float(hid) < self.tau_fast:
            return Verdict.ACCEPT, float(hid), "hide_fast"

        icr = scores.get("icr")
        early = [x for x in (ent, hid, icr) if x is not None and _finite(float(x))]
        if len(early) >= 2:
            m = float(sum(float(x) for x in early) / len(early))
            if m < self.tau_verify:
                return Verdict.ACCEPT, m, "entropy_hide_icr_mean"

        pool: list[float] = []
        for k in ("entropy", "hide", "icr", "lsd", "spectral", "sae"):
            v = scores.get(k)
            if v is None or not _finite(float(v)):
                continue
            pool.append(float(v))
        if not pool:
            return Verdict.ABSTAIN, 0.5, "no_signals"
        combined = float(sum(pool) / len(pool))
        st = SigmaState()
        k_raw = max(0.0, min(1.0, 1.0 - combined))
        sigma_update(st, combined, k_raw)
        return sigma_gate(st), combined, "cognitive_pool"

    def __call__(self, scores: Mapping[str, Optional[float]]) -> Tuple[Verdict, float, str]:
        """Alias of :meth:`cascade` for harness-style ``fus(scores)`` calls."""
        return self.cascade(scores)


__all__ = [
    "DEFAULT_WEIGHTS",
    "SigmaFusion",
    "entropy_sigma_from_logprobs",
    "fused_sigma",
    "merge_weight_dict",
    "normalize_weights",
]
