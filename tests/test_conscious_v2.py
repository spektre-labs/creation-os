# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import pytest

from cos.conscious import SigmaConsciousV2
from cos.sigma_gate import SigmaGate


def test_metacognitive_vector_5_dims() -> None:
    s = SigmaConsciousV2()
    v = s.metacognitive_vector("What is 2+2?", "4")
    for k in ("coherence", "confidence", "novelty", "complexity", "alignment", "σ_meta", "action"):
        assert k in v
    assert isinstance(v["σ_meta"], float)


def test_predict_own_sigma() -> None:
    s = SigmaConsciousV2()
    out = s.predict_own_σ("short", "reply")
    assert "predicted_σ" in out and "actual_σ" in out
    assert "gap" in out
    assert len(s.σ_predictions) == 1


def test_calibration_score() -> None:
    gate = SigmaGate()

    class _Fixed:
        def score(self, _p: str, _r: str) -> tuple[float, str]:
            return 0.4, "ACCEPT"

    s = SigmaConsciousV2(gate=_Fixed())  # type: ignore[arg-type]
    for i in range(5):
        s.predict_own_σ(f"word {i}", "ok")
    cal = s.calibration_score()
    assert cal["n_samples"] == 5
    assert "calibration" in cal
    assert cal["avg_gap"] is not None


def test_selective_engagement() -> None:
    s = SigmaConsciousV2()
    r = s.selective_engagement("hello", "hi there")
    assert "engaged" in r and "verdict" in r and "σ" in r
    assert "note" in r


def test_well_calibrated_low_gap(monkeypatch: pytest.MonkeyPatch) -> None:
    gate = SigmaGate()

    def tight(_p: str, _r: str) -> tuple[float, str]:
        return 0.31, "ACCEPT"

    monkeypatch.setattr(gate, "score", tight)
    s = SigmaConsciousV2(gate=gate)
    out = s.predict_own_σ("no question marks few words", "x")
    assert out["well_calibrated"] is True


def test_overconfident_detected(monkeypatch: pytest.MonkeyPatch) -> None:
    gate = SigmaGate()

    def high_stress(_p: str, _r: str) -> tuple[float, str]:
        return 0.95, "ABSTAIN"

    monkeypatch.setattr(gate, "score", high_stress)
    s = SigmaConsciousV2(gate=gate)
    out = s.predict_own_σ("tiny", "x")
    assert out["overconfident"] is True


def test_dunning_kruger_check(monkeypatch: pytest.MonkeyPatch) -> None:
    gate = SigmaGate()

    def hi(_p: str, _r: str) -> tuple[float, str]:
        return 0.9, "ABSTAIN"

    monkeypatch.setattr(gate, "score", hi)
    s = SigmaConsciousV2(gate=gate)
    for i in range(10):
        s.predict_own_σ(f"easy {i}", "a")
    dk = s.dunning_kruger_check()
    assert dk.get("dunning_kruger") is True
    assert dk["overconfident_count"] > 0


def test_action_recommendation() -> None:
    s = SigmaConsciousV2()
    v = s.metacognitive_vector("What is 2+2?", "4")
    assert v["action"] in (
        "proceed_confidently",
        "proceed_with_caution",
        "think_more_carefully",
        "seek_help_or_defer",
        "abstain_and_explain",
    )
