# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for ``cos.probe`` (no real HF models required)."""
from __future__ import annotations

from cos.probe import SigmaProbe, SignalCascade


def test_cascade_l1_only(cascade) -> None:
    result = cascade.score("test", "hello world")
    assert "sigma" in result
    assert "verdict" in result
    assert "L1_entropy" in result["levels"]
    assert result["cascade_depth"] >= 1


def test_cascade_empty_response(cascade) -> None:
    result = cascade.score("test", "")
    assert result["sigma"] == 1.0
    assert result["verdict"] == "ABSTAIN"


def test_cascade_normal_text(cascade) -> None:
    result = cascade.score("What is AI?", "Artificial intelligence is a broad field of computer science")
    assert 0 <= result["sigma"] <= 1
    assert result["verdict"] in ("ACCEPT", "RETHINK", "ABSTAIN")


def test_cascade_cost_saved(cascade) -> None:
    result = cascade.score("test", "hello")
    assert "cost_saved" in result


def test_cascade_early_exit() -> None:
    cascade = SignalCascade(thresholds={"early_accept": 0.5, "early_abstain": 0.9})
    result = cascade.score("test", "normal text here with some variation")
    assert result["cascade_depth"] >= 1


def test_probe_no_model() -> None:
    probe = SigmaProbe(model=None)
    result = probe.extract(None)
    assert result["hidden_states"] is None
