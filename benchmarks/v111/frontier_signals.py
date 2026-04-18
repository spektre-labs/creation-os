# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2026 Spektre Labs / Lauri Rainio
"""
v111 — frontier-matrix gating signals.

Six signals are explicitly enumerated in the v111 directive:

    1. entropy          — classical selective-prediction baseline
                          (channel 0 of the σ profile, = H(softmax)/log V).
    2. sigma_mean       — v103 default (arithmetic mean of 8 channels).
    3. sigma_max_token  — v103 secondary (max over tokens of mean-channel σ).
    4. sigma_product    — v104 winner (geometric mean of 8 channels).
    5. sigma_tail_mass  — channel 3: probability mass outside top-20 tokens.
    6. sigma_n_effective — channel 6: 1 − exp(H) / V (effective support frac.).

v111 tests the `5 σ-candidates vs. entropy-baseline` matrix across four
task families (arc-class, truthfulqa, hellaswag, mmlu / gsm8k / humaneval
pending) with a paired bootstrap ΔAURCC. Multiple-comparison correction
is Bonferroni with N = 4 tasks × 6 signals = 24.

All signals consume the SAME v103 sidecar row shape (see v104/operators.py
docstring for the exact schema).  No hidden state; no dataset leakage;
no post-hoc signal tuning.
"""

from __future__ import annotations

import math
from typing import Callable, Dict


def _clamp01(x: float) -> float:
    return max(0.0, min(1.0, x))


def op_entropy(row: dict) -> float:
    """Baseline: channel 0 = entropy_norm = H(softmax) / log(V)."""
    return float(row["sigma_profile"][0])


def op_sigma_mean(row: dict) -> float:
    """v103 default — mean of 8 channels (already averaged over tokens)."""
    return float(row["sigma_mean"])


def op_sigma_max_token(row: dict) -> float:
    """v103 secondary — max per-token of scalar σ (mean over channels)."""
    return float(row["sigma_max_token"])


def op_sigma_product(row: dict) -> float:
    """v104 winner — geometric mean of 8 channels with 1e-6 floor.

    Matches `src/v101/sigma_channels.c::cos_v101_sigma_from_logits`
    which is now the v105 default aggregator.
    """
    eps = 1e-6
    p = row["sigma_profile"]
    lp = [math.log(max(float(x), eps)) for x in p]
    return float(math.exp(sum(lp) / len(lp)))


def op_sigma_tail_mass(row: dict) -> float:
    """Channel 3 — probability mass outside the top-20 softmax tokens."""
    return float(row["sigma_profile"][3])


def op_sigma_n_effective(row: dict) -> float:
    """Channel 6 — 1 - exp(H) / V, the normalised effective support size."""
    return float(row["sigma_profile"][6])


# Registry — exact order is pre-registered and matches
# docs/v111/THE_FRONTIER_MATRIX.md §3.  Do not reorder after merge.
SIGNALS: Dict[str, Callable[[dict], float]] = {
    "entropy":          op_entropy,            # baseline
    "sigma_mean":       op_sigma_mean,
    "sigma_max_token":  op_sigma_max_token,
    "sigma_product":    op_sigma_product,
    "sigma_tail_mass":  op_sigma_tail_mass,
    "sigma_n_effective": op_sigma_n_effective,
}

# How many comparisons enter the Bonferroni correction for H_op.
#   5 σ-signals (all except baseline) × 4 task families = 20 per-task-per-signal
#   + global cross-task best-operator H0        =  4
#   ------------------------------------------------------
#   = 24 family-wise comparisons
BONFERRONI_N = 24

# Default task families the matrix spans.  Fourth family is HellaSwag
# (loglikelihood, multi-choice, sits in the same v103 backend pipeline as
# ARC / TruthfulQA).  MMLU / GSM8K / HumanEval are documented in
# run_mmlu_gsm8k_humaneval.sh with exact repro commands; they are not
# required for the matrix to render — missing families appear as PENDING
# rows in the markdown output.
TASK_FAMILIES = [
    "arc_challenge",
    "arc_easy",
    "truthfulqa_mc2",
    "hellaswag",
]
