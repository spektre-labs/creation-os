# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for ``cos.corvus_defense`` (multi-probe consensus lab)."""
from __future__ import annotations

from cos.corvus_defense import SigmaCorvusDefense


def test_detect_signal_masking_benign() -> None:
    d = SigmaCorvusDefense()

    def probe(_: object) -> float:
        return 0.5

    r = d.detect_signal_masking({}, {}, probe=probe)
    assert r["ok"] is True
    assert r["suspicious"] is False
    assert r["delta"] == 0.0


def test_detect_signal_masking_large_delta() -> None:
    d = SigmaCorvusDefense()
    r = d.detect_signal_masking("x", "y", probe=lambda m: 0.9 if m == "x" else 0.2)
    assert r["suspicious"] is True
    assert r["delta"] > 0.35


def test_multi_signal_consensus_suspicious_split() -> None:
    d = SigmaCorvusDefense()
    scores = {"sink": 0.05, "icr": 0.65, "sep": 0.66, "spectral": 0.95}
    r = d.multi_signal_consensus(scores)
    assert r["verdict"] == "RETHINK"
    assert r["suspicious"] is True
    assert "sink" in r["outlier_low"]


def test_multi_signal_consensus_accept_similar() -> None:
    d = SigmaCorvusDefense()
    scores = {"a": 0.42, "b": 0.45, "c": 0.44}
    r = d.multi_signal_consensus(scores)
    assert r["verdict"] == "ACCEPT"
    assert r["suspicious"] is False


def test_probe_diversity_and_integrity() -> None:
    d = SigmaCorvusDefense()
    div = d.probe_diversity(("sink_probe", "icr_lane", "spectral_v5", "entropy_l1"))
    assert div["score"] >= 0.99
    ok = SigmaCorvusDefense.sigma_integrity_check("abc", "abc")
    bad = SigmaCorvusDefense.sigma_integrity_check("abc", "abz")
    assert ok["ok"] is True and bad["ok"] is False


def test_canary_injection_returns_structure() -> None:
    d = SigmaCorvusDefense()
    r = d.canary_injection("Say only YES.", "YES", min_sigma=0.0)
    assert "sigma" in r and "verdict" in r
    assert "probe_compromised_suspect" in r
