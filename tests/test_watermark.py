# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""Tests for :mod:`cos.watermark`."""
from __future__ import annotations

from cos import SigmaGate
from cos.watermark import SigmaWatermark


def test_watermark_embed_detect_roundtrip() -> None:
    wm = SigmaWatermark()
    t = "The quick brown fox."
    e = wm.embed(t, "k1")
    d = wm.detect(e, "k1")
    assert d["watermarked"] is True
    assert d["confidence"] > 0.5


def test_watermark_wrong_key_fails() -> None:
    wm = SigmaWatermark()
    e = wm.embed("abc", "secret")
    d = wm.detect(e, "other")
    assert d["watermarked"] is False


def test_watermark_sigma_provenance() -> None:
    wm = SigmaWatermark()
    gate = SigmaGate()
    e = wm.embed("Hi", "k")
    p = wm.sigma_provenance(e, "k", gate)
    assert "gate_verdict" in p
    assert "provenance_sigma" in p


def test_watermark_strip_attempt() -> None:
    wm = SigmaWatermark()
    raw = wm.embed("text", "k")
    damaged = raw[:-3]  # chop end of tag
    s = wm.strip_attempt_detection(damaged, "k")
    assert s["likely_strip_attempt"] is True


def test_watermark_missing_tag() -> None:
    wm = SigmaWatermark()
    d = wm.detect("no watermark here", "k")
    assert d["watermarked"] is False
    assert d["sigma"] > 0.5
