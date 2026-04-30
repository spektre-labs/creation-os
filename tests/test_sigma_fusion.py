# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.sigma_fusion import (  # noqa: E402
    DEFAULT_WEIGHTS,
    SigmaFusion,
    entropy_sigma_from_logprobs,
    fused_sigma,
    merge_weight_dict,
    normalize_weights,
)
from cos.sigma_gate_core import Verdict  # noqa: E402


def test_fused_sigma_max() -> None:
    s = fused_sigma({"lsd": 0.2, "hide": 0.9}, mode="max")
    assert abs(s - 0.9) < 1e-9


def test_fused_sigma_weighted_mean_default_weights() -> None:
    sigs = {"lsd": 0.0, "hide": 0.0, "icr": 0.0, "entropy": 1.0}
    out = fused_sigma(sigs, DEFAULT_WEIGHTS, mode="weighted_mean")
    assert abs(out - 0.15) < 1e-6


def test_normalize_weights() -> None:
    w = normalize_weights({"a": 1.0, "b": 3.0})
    assert abs(w["a"] - 0.25) < 1e-9 and abs(w["b"] - 0.75) < 1e-9


def test_merge_weight_dict() -> None:
    m = merge_weight_dict(DEFAULT_WEIGHTS, {"lsd": 2.0})
    assert abs(sum(m.values()) - 1.0) < 1e-9
    assert m["lsd"] > DEFAULT_WEIGHTS["lsd"]


def test_entropy_sigma_uniform_high() -> None:
    import numpy as np

    lp = np.log(np.ones(8) / 8.0)
    s = entropy_sigma_from_logprobs(lp)
    assert s > 0.9


def test_sigma_fusion_cascade_entropy_fast() -> None:
    fus = SigmaFusion(tau_fast=0.5, tau_verify=0.6)
    v, pooled, note = fus.cascade({"entropy": 0.1, "hide": 0.9, "icr": 0.9})
    assert v == Verdict.ACCEPT
    assert abs(pooled - 0.1) < 1e-9
    assert note == "entropy_fast"


def test_sigma_fusion_cascade_mean_then_gate() -> None:
    fus = SigmaFusion(tau_fast=0.05, tau_verify=0.05)
    v, pooled, note = fus.cascade({"entropy": 0.4, "hide": 0.4, "icr": 0.4, "lsd": 0.4})
    assert note == "cognitive_pool"
    assert abs(pooled - 0.4) < 1e-9
    assert v in (Verdict.ACCEPT, Verdict.RETHINK, Verdict.ABSTAIN)


def test_sigma_fusion_pool_includes_sae_last() -> None:
    fus = SigmaFusion(tau_fast=0.01, tau_verify=0.01)
    v, pooled, note = fus.cascade(
        {"entropy": 0.5, "hide": 0.5, "icr": 0.5, "lsd": 0.5, "spectral": 0.4, "sae": 0.9}
    )
    assert note == "cognitive_pool"
    assert abs(pooled - (0.5 * 4 + 0.4 + 0.9) / 6.0) < 1e-6
    assert v in (Verdict.ACCEPT, Verdict.RETHINK, Verdict.ABSTAIN)
