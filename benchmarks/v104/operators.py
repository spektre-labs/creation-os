# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2026 Spektre Labs / Lauri Rainio
"""
v104 — σ-aggregation operators.

Ten pre-registered candidates (plus the entropy baseline) for how to
reduce a σ profile into a single per-candidate scalar that is then used
as a selective-prediction gating signal.  All operators consume exactly
the same v103 sidecar JSONL row:

    {
      "sigma_mean":       float,                # v103 default
      "sigma_max_token":  float,                # v103 secondary
      "sigma_profile":    [e, m, tk, tl, ls, pm, ne, std],
                                                # aggregated over tokens
                                                # ALREADY — so per-channel
                                                # per-token max is not
                                                # available at analysis time
                                                # unless re-logged.
    }

The v103 bridge aggregates the eight channels by *mean over tokens*
before writing the sidecar.  Any operator here that needs per-token
information can use only `sigma_mean` (mean over both tokens and
channels) and `sigma_max_token` (max over tokens of the mean-over-
channels scalar σ).  Channel-level operators (max_channel, product,
l2, topk, per_channel) use the 8-vector which is itself a mean over
tokens.  An n=5000 re-logging pass that also persists the full token ×
channel matrix would enable strictly more operators; for now we work
with what the existing sidecar contains.

Pre-registration: these ten operators, in this order, are fixed before
we look at the n=5000 numbers.  No additions after the fact.
"""

from __future__ import annotations

import math
from typing import Callable, Dict


def _clamp01(x: float) -> float:
    return max(0.0, min(1.0, x))


# -------------------------- candidate operators ------------------------- #

def op_sigma_mean(row: dict) -> float:
    """v103 default — mean of 8 channels, averaged over tokens.  Measured null."""
    return float(row["sigma_mean"])


def op_sigma_max_token(row: dict) -> float:
    """v103 secondary — max per token of the scalar σ.  Suggestive winner at n=500."""
    return float(row["sigma_max_token"])


def op_sigma_max_channel(row: dict) -> float:
    """Max over the 8 token-averaged channels."""
    p = row["sigma_profile"]
    return float(max(p))


def op_sigma_topk_mean_k3(row: dict) -> float:
    """Mean of the top-3 (most uncertain) of the 8 token-averaged channels."""
    p = sorted(row["sigma_profile"], reverse=True)
    return float(sum(p[:3]) / 3.0)


def op_sigma_p95_token(row: dict) -> float:
    """95th-percentile-approximation of σ over tokens.

    Note: we only have `sigma_max_token` (P100) and `sigma_mean` (mean) at
    sidecar level, so we form a convex combination as a proxy:
        0.75 * sigma_max_token + 0.25 * sigma_mean
    This overshoots P95 on uniform tails and undershoots on heavy tails;
    if it wins anywhere, the v104.5 re-log will nail the true P95.
    """
    return _clamp01(0.75 * row["sigma_max_token"] + 0.25 * row["sigma_mean"])


def op_sigma_product(row: dict) -> float:
    """Geometric mean of the 8 channels (in [0,1]).  Punishes any low channel."""
    p = row["sigma_profile"]
    # geometric mean on [eps, 1] to avoid log(0)
    eps = 1e-6
    lp = [math.log(max(float(x), eps)) for x in p]
    return float(math.exp(sum(lp) / len(lp)))


def op_sigma_l2(row: dict) -> float:
    """L2 norm of the 8-channel vector, normalised to [0,1] by sqrt(8)."""
    p = row["sigma_profile"]
    s2 = sum(float(x) * float(x) for x in p)
    return _clamp01(math.sqrt(s2) / math.sqrt(8.0))


def op_sigma_max_of_max(row: dict) -> float:
    """Max over channels of token-max.  Approximation: we only have
    token-max of the scalar σ (= mean over channels), not per-channel
    token-max.  Proxy: max(sigma_max_token, max(sigma_profile))."""
    return _clamp01(max(float(row["sigma_max_token"]), float(max(row["sigma_profile"]))))


def op_sigma_entropy_hybrid(row: dict) -> float:
    """Pre-registered hybrid: 0.5·sigma_max_token + 0.5·entropy_channel.

    Directly motivated by arXiv:2603.21172: entropy alone is insufficient;
    add a second signal.  Here σ_max_token is the second signal.
    """
    ent = float(row["sigma_profile"][0])  # entropy_norm
    return _clamp01(0.5 * float(row["sigma_max_token"]) + 0.5 * ent)


def op_sigma_per_channel_best(row: dict) -> float:
    """Placeholder.  The real per-channel-best-oracle is found by
    `channel_analysis.py` at global task level (best channel on the
    training half, evaluated on the held-out half).  This operator is
    the *channel whose index is fixed after channel analysis*; until
    then it falls back to entropy (channel 0).  The operator search
    compares it honestly with the entropy baseline *after* the channel
    index is picked.  We mark runs using it with a provenance flag.
    """
    return float(row["sigma_profile"][0])


# ------------------------ baselines / controls -------------------------- #

def op_entropy_only(row: dict) -> float:
    """Single channel 0 of the σ profile.  The classical selective-prediction
    baseline (and the v103 reference signal that σ_mean tied)."""
    return float(row["sigma_profile"][0])


def op_margin_only(row: dict) -> float:
    """Channel 1 of the σ profile — (1 - (p_top1 - p_top2))."""
    return float(row["sigma_profile"][1])


def op_p_max_only(row: dict) -> float:
    """Channel 5 of the σ profile — (1 - p_top1)."""
    return float(row["sigma_profile"][5])


def op_random(row: dict) -> float:
    """Seed-derived random for floor comparison.  Deterministic over a
    single run so bootstrap statistics are meaningful; we inject the seed
    by hashing (doc_index, cand_index, 'v104')."""
    key = (row.get("doc_index", 0), row.get("cand_index", 0), "v104")
    h = hash(repr(key)) & 0xFFFFFFFF
    return (h / 0xFFFFFFFF)


# ---------------------- registry (order matters) ------------------------ #

OPERATORS: Dict[str, Callable[[dict], float]] = {
    # 10 pre-registered candidates
    "sigma_mean":            op_sigma_mean,
    "sigma_max_token":       op_sigma_max_token,
    "sigma_max_channel":     op_sigma_max_channel,
    "sigma_topk_mean_k3":    op_sigma_topk_mean_k3,
    "sigma_p95_token":       op_sigma_p95_token,
    "sigma_product":         op_sigma_product,
    "sigma_l2":              op_sigma_l2,
    "sigma_max_of_max":      op_sigma_max_of_max,
    "sigma_entropy_hybrid":  op_sigma_entropy_hybrid,
    "sigma_per_channel_best": op_sigma_per_channel_best,
    # baselines / controls
    "entropy":               op_entropy_only,
    "margin":                op_margin_only,
    "p_max":                 op_p_max_only,
    "random":                op_random,
}

# Operators counted in the Bonferroni correction for H1 multiple comparison
# (only the 10 σ candidates, NOT the baselines).
BONFERRONI_N = 10
