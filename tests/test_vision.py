# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.vision`."""
from __future__ import annotations

from cos.sigma_gate import SigmaGate
from cos.vision import SigmaVision


def test_score_vlm_keys() -> None:
    v = SigmaVision()
    r = v.score_vlm(b"x", "What color?", "The car is red.", SigmaGate())
    assert "sigma_text" in r and "sigma_grounding" in r and "sigma_combined" in r


def test_attention_to_image_ratio_mapping() -> None:
    m = {"image": [0.8, 0.2], "text": [0.1, 0.9]}
    r = SigmaVision.attention_to_image_ratio(m)
    assert r["ratio"] is not None and "low_ratio" in str(r).lower()


def test_visual_grounding_check() -> None:
    g = SigmaVision.visual_grounding_check("I see a red car", None, ["red car", "bus"])
    assert g["grounded_ok"] is True


def test_sigma_per_token_visual() -> None:
    rows = SigmaVision.sigma_per_token_visual(["The", "red"], [0.8, 0.1])
    assert len(rows) == 2 and rows[0]["looks_at_image"] is True


def test_grounded_cot_steps() -> None:
    v = SigmaVision()
    out = v.grounded_cot(
        "Describe",
        b"",
        SigmaGate(),
        regions=[{"id": "roi1", "box": [0, 0, 0.5, 0.5]}],
    )
    assert len(out["steps"]) >= 1


def test_modalities_tuple() -> None:
    assert "image" in SigmaVision.modalities


def test_attention_none() -> None:
    r = SigmaVision.attention_to_image_ratio(None)
    assert r.get("ratio") is None
