# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2026 Spektre Labs / Lauri Rainio
"""
v111 — adaptive / composite σ signals (post-hoc exploration).

Context
-------
The six `frontier_signals.SIGNALS` entries are **pre-registered**:
their order, number, and Bonferroni N = 24 were fixed before the v104
n=5000 σ-sidecars were analysed.  Adding any new signal to that
registry mid-flight would inflate Type-I error.

Instead, this file defines a SEPARATE set of signals that v111.2
exploration may evaluate on exactly the same sidecars, with a SEPARATE
Bonferroni correction and with every row in the resulting table
labelled "post-hoc exploration (not pre-registered)".

Per CLAIM_DISCIPLINE: numbers produced from this registry may be
reported, but ONLY alongside the pre-registered v111 matrix and ONLY
under the "post-hoc" label.  They may NOT be substituted into the
pre-registered claim.

Rationale for each candidate
---------------------------
* `sigma_composite_max`  = max(σ_max_token, entropy).
    Conservative abstain policy: if EITHER signal is unsure, abstain.
    Motivated by the HellaSwag result where entropy is the strong
    baseline and σ_max_token actively hurts — `max` should never be
    worse than the weaker of the two on average, and often beats
    either alone when the two signals err on different subsets.

* `sigma_composite_mean` = 0.5·(σ_max_token + entropy).
    The linear-blend counterpart; a lower-variance estimator of
    epistemic uncertainty when the two channels are ~uncorrelated.

* `sigma_task_adaptive`   = per-task pre-specified choice:
      truthfulqa_mc2 → sigma_max_token     (pre-registered winner)
      hellaswag       → entropy             (hellaswag-strong baseline)
      arc_challenge   → sigma_product       (v104 winner)
      arc_easy        → entropy             (matrix-best, near-tied)
      else            → sigma_product       (v104 default)
    This encodes a CAPABILITY: the router must know the task.
    The table reports it as a *post-hoc upper bound* on what
    task-aware routing could deliver; no router yet exists that
    classifies tasks at inference time.

The `sigma_task_adaptive` signal is not a scalar of a single sidecar
row — it needs the task name.  It is registered here for plumbing
completeness; the driver in `adaptive_matrix.py` special-cases it.

Bonferroni N for this exploratory matrix
----------------------------------------
    3 adaptive signals × 4 task families = 12
Exploratory α_fw = 0.05 / 12 = 0.00417.  This is reported in the
markdown output, never merged with the pre-registered α.
"""

from __future__ import annotations

from typing import Callable, Dict

from frontier_signals import op_entropy, op_sigma_max_token, op_sigma_product


def op_sigma_composite_max(row: dict) -> float:
    """max(σ_max_token, entropy) — EITHER unsure ⇒ abstain."""
    return max(op_sigma_max_token(row), op_entropy(row))


def op_sigma_composite_mean(row: dict) -> float:
    """0.5·(σ_max_token + entropy) — linear blend."""
    return 0.5 * (op_sigma_max_token(row) + op_entropy(row))


def _dispatch(task: str, row: dict) -> float:
    """Per-task pre-specified signal.  Driver must pass task name."""
    if task == "truthfulqa_mc2":
        return op_sigma_max_token(row)
    if task == "hellaswag":
        return op_entropy(row)
    if task == "arc_challenge":
        return op_sigma_product(row)
    if task == "arc_easy":
        return op_entropy(row)
    return op_sigma_product(row)


# Registry — post-hoc exploration, not pre-registered.
ADAPTIVE_SIGNALS: Dict[str, Callable[[dict], float]] = {
    "sigma_composite_max":  op_sigma_composite_max,
    "sigma_composite_mean": op_sigma_composite_mean,
    # sigma_task_adaptive is a family-routed signal; the driver calls
    # `_dispatch(task, row)` directly rather than pulling from this dict.
}


ADAPTIVE_TASK_ROUTED = "sigma_task_adaptive"

ADAPTIVE_BONFERRONI_N = (
    len(ADAPTIVE_SIGNALS) + 1   # + task-routed
) * 4                            # × 4 task families

ADAPTIVE_PREREGISTERED = False   # explicit label written into the JSON.


def adaptive_signal_for(task: str, row: dict) -> Dict[str, float]:
    """Compute every adaptive signal (post-hoc) for one sidecar row."""
    out: Dict[str, float] = {}
    for name, fn in ADAPTIVE_SIGNALS.items():
        out[name] = float(fn(row))
    out[ADAPTIVE_TASK_ROUTED] = float(_dispatch(task, row))
    return out
