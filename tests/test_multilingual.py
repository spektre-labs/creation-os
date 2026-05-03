# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""Tests for :mod:`cos.multilingual`."""
from __future__ import annotations

from cos import SigmaGate
from cos.multilingual import SUPPORTED_LANGUAGES, SigmaMultilingual


def test_multilingual_supported_list() -> None:
    assert "fi" in SUPPORTED_LANGUAGES and "en" in SUPPORTED_LANGUAGES


def test_multilingual_detect_fi() -> None:
    m = SigmaMultilingual()
    r = m.detect_language("Tämä on suomeksi ja minä olen täällä")
    assert r["language"] == "fi"
    assert r["confidence"] > 0.4


def test_multilingual_detect_en() -> None:
    m = SigmaMultilingual()
    r = m.detect_language("the quick brown fox who is hello")
    assert r["language"] == "en"


def test_multilingual_sigma_per_language() -> None:
    m = SigmaMultilingual()
    gate = SigmaGate()
    r = m.sigma_per_language("hello there", gate)
    assert r["language"] == "en"
    assert "sigma_adjusted" in r


def test_multilingual_cross_lingual_calibrate() -> None:
    m = SigmaMultilingual()
    gate = SigmaGate()
    data = {"en": [("q", "a")], "fi": [("kysy", "vast")]}
    out = m.cross_lingual_calibrate(gate, data)
    assert "en" in out["languages"]
    r = m.sigma_translation_quality("source text", "target text", gate)
    assert "sigma" in r
