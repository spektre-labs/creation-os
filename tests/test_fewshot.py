# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import pytest

from cos.fewshot import SigmaFewShot, _HAS_HDC


def test_learn_class_from_examples() -> None:
    fs = SigmaFewShot(dim=256, use_hdc=False)
    out = fs.learn("cats", ["small furry pet", "meows often"])
    assert out.get("learned") is True
    assert out["class"] == "cats"


def test_classify_returns_best_match() -> None:
    fs = SigmaFewShot(dim=256, use_hdc=False)
    fs.learn("dogs", ["barks loud", "chases balls"])
    fs.learn("cats", ["meows", "climbs trees"])
    r = fs.classify("the animal meows softly")
    assert r.get("class") == "cats"


def test_classify_unknown_abstains() -> None:
    fs = SigmaFewShot()
    r = fs.classify("anything")
    assert r.get("class") is None
    assert r.get("verdict") == "ABSTAIN"


@pytest.mark.skipif(not _HAS_HDC, reason="numpy/hypervector not available")
def test_hdc_encode_deterministic() -> None:
    from cos.hypervector import HyperVector

    fs1 = SigmaFewShot(dim=256, codebook_seed=42)
    fs2 = SigmaFewShot(dim=256, codebook_seed=42)
    a = fs1._encode_text("alpha beta gamma")
    b = fs2._encode_text("alpha beta gamma")
    assert HyperVector.similarity(a, b) > 0.999


@pytest.mark.skipif(not _HAS_HDC, reason="numpy/hypervector not available")
def test_hdc_prototype_bundles() -> None:
    fs = SigmaFewShot(dim=256)
    fs.learn("topicA", ["foo bar", "foo baz"])
    assert fs.classes["topicA"]["method"] == "hdc"
    r = fs.classify("foo qux")
    assert r.get("class") == "topicA"
    assert "similarity" in r


def test_overlap_fallback_works() -> None:
    fs = SigmaFewShot(dim=256, use_hdc=False)
    fs.learn("A", ["x y z", "x y w"])
    r = fs.classify("x y new")
    assert r["class"] == "A"
    assert r["σ"] < 1.0


def test_transfer_check_measures_improvement() -> None:
    fs = SigmaFewShot(dim=256, use_hdc=False)
    rep = fs.transfer_check(
        source_examples=["shared token alpha beta", "shared token alpha gamma"],
        target_examples=["shared token alpha delta"],
    )
    assert "σ_before" in rep and "σ_after" in rep
    assert rep["transferred"] is True
    assert rep["improvement"] >= 0.0


def test_adapt_returns_report() -> None:
    fs = SigmaFewShot(dim=256, use_hdc=False)
    rep = fs.adapt(
        [
            ("p1", "r1", True),
            ("p2", "r2", False),
        ],
    )
    assert rep["n_examples"] == 2
    assert "σ_before_avg" in rep and "σ_after_avg" in rep
    assert "improved" in rep
