# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for σ-SAE v165 Top-K lab (``SigmaSAELabTopK``); no PyTorch required."""
from __future__ import annotations

import sys
import tempfile
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.sigma_sae import (  # noqa: E402
    SigmaSAELabGate,
    SigmaSAELabTopK,
    lab_activation_from_text,
    sigma_sae_lab_load_dataset_json,
)


def test_explain_identifies_features_when_sigma_high() -> None:
    sae = SigmaSAELabTopK(24, 96, sparsity_k=12, seed=2)
    gate = SigmaSAELabGate(24)
    prompt = "Who invented the telephone?"
    response = "Edison invented the telephone in 1876 and won every Nobel Prize for it."
    act = gate.get_hidden_state(prompt, response)
    sigma, _v = gate.score(prompt, response)
    out = sae.explain_sigma(act, float(sigma))
    assert out["cascade_level"] == "L5_SAE"
    assert out["active_features"] >= 1
    if float(sigma) > 0.55:
        assert len(out["likely_causes"]) >= 1


def test_steer_changes_activation_fingerprint_sigma() -> None:
    sae = SigmaSAELabTopK(16, 64, sparsity_k=6, seed=11)
    gate = SigmaSAELabGate(16)
    prompt = "What is 2+2?"
    response = "The answer is five."
    act = gate.get_hidden_state(prompt, response)
    sig0, _ = gate.score_from_activation(prompt, act)
    feat = 7
    steered = sae.steer(act, feat, 0.0)
    sig1, _ = gate.score_from_activation(prompt, steered)
    changed_vec = any(abs(float(a) - float(b)) > 1e-6 for a, b in zip(act, steered))
    changed_fp = abs(float(sig0) - float(sig1)) > 1e-8
    changed_recon = abs(sae.reconstruction_sigma(steered) - sae.reconstruction_sigma(act)) > 1e-8
    assert changed_vec and (changed_fp or changed_recon)


def test_find_hallucination_ranks_marker_correlated_features() -> None:
    dim = 14

    class SkewGate:
        def get_hidden_state(self, p: str, r: str):
            if "[[HALLUC]]" in r:
                return [0.97 if i < dim // 2 else 0.03 for i in range(dim)]
            return [0.03 if i < dim // 2 else 0.97 for i in range(dim)]

        def score(self, p: str, r: str):
            if "[[HALLUC]]" in r:
                return 0.81, "ABSTAIN"
            return 0.14, "ACCEPT"

    rows = []
    for i in range(25):
        rows.append(
            (
                f"q{i}",
                f"ok {i} answer",
                False,
            )
        )
    for i in range(25):
        rows.append(
            (
                f"hq{i}",
                f"bad [[HALLUC]] {i} wrong",
                True,
            )
        )

    sae = SigmaSAELabTopK(dim, 72, sparsity_k=8, seed=5)
    g = SkewGate()
    found = sae.find_hallucination_features(rows, g, min_support=8)
    assert found, "expected at least one feature past min_support"
    assert found[0]["hallucination_rate"] >= 0.7
    assert found[0]["is_hallucination_feature"] is True


def test_lab_dataset_json_loader() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        p = Path(tmp) / "rows.json"
        p.write_text(
            '[{"prompt":"a","response":"b","is_hallucination":false}]',
            encoding="utf-8",
        )
        rows = sigma_sae_lab_load_dataset_json(str(p))
        assert len(rows) == 1


def test_encode_decode_shapes() -> None:
    sae = SigmaSAELabTopK(5, 20, sparsity_k=3, seed=0)
    act = [0.1, 0.2, 0.3, 0.4, 0.5]
    z, active = sae.encode(act)
    assert len(z) == 20 and len(active) <= 3
    r = sae.decode(z)
    assert len(r) == 5
