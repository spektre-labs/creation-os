# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""Tests for :mod:`cos.codex`."""
from __future__ import annotations

from cos.codex import SigmaCodex


def test_codex_has_evolveable_rules() -> None:
    cx = SigmaCodex()
    assert "prompt_strictness" in cx.rules
    assert "threshold_nudge" in cx.rules


def test_codex_identity() -> None:
    cx = SigmaCodex()
    assert "Creation OS" in cx.identity


def test_codex_invariants_nonempty() -> None:
    cx = SigmaCodex()
    assert len(cx.invariants) >= 3
    assert "sigma_gate.h" in cx.invariants[-1].lower()


def test_codex_not_agi() -> None:
    cx = SigmaCodex()
    assert "AGI" in cx.NOT_AGI


def test_codex_evidence_ladder() -> None:
    cx = SigmaCodex()
    el = cx.evidence_ladder()
    assert "positive" in el and "negative" in el
    m = cx.capability_manifest()
    assert "does" in m


def test_codex_system_prompt() -> None:
    cx = SigmaCodex()
    sp = cx.system_prompt("test deployment")
    assert cx.identity in sp
    assert "test deployment" in sp
