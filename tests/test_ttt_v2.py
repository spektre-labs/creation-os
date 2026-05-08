# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import pytest

from cos.sigma_gate import SigmaGate
from cos.ttt import SigmaTTT


def test_fast_path_low_sigma(monkeypatch: pytest.MonkeyPatch) -> None:
    gate = SigmaGate()

    def low(_p: str, _r: str) -> tuple[float, str]:
        return 0.1, "ACCEPT"

    monkeypatch.setattr(gate, "score", low)
    t = SigmaTTT(gate=gate, trigger_threshold=0.4)
    out = t.process("q", "a")
    assert out["path"] == "FAST"
    assert out["adapted"] is False
    assert out["σ"] == pytest.approx(0.1, abs=0.01)


def test_slow_path_high_sigma(monkeypatch: pytest.MonkeyPatch) -> None:
    gate = SigmaGate()
    n = {"i": 0}

    def seq(_p: str, r: str) -> tuple[float, str]:
        if "adapted step" in r:
            return 0.1, "ACCEPT"
        n["i"] += 1
        return 0.95, "ABSTAIN"

    monkeypatch.setattr(gate, "score", seq)
    t = SigmaTTT(gate=gate, trigger_threshold=0.4, max_steps=5)
    out = t.process("question here", "bad")
    assert out["path"] == "ADAPTED"
    assert out["adapted"] is True
    assert out["σ"] < 0.4


def test_adaptation_reduces_sigma(monkeypatch: pytest.MonkeyPatch) -> None:
    gate = SigmaGate()
    fake_sigma = [0.9, 0.85, 0.1]
    idx = {"k": 0}

    def sc(_p: str, _r: str) -> tuple[float, str]:
        i = min(idx["k"], len(fake_sigma) - 1)
        idx["k"] += 1
        s = fake_sigma[i]
        return s, "ABSTAIN" if s > 0.5 else "ACCEPT"

    monkeypatch.setattr(gate, "score", sc)

    def adapt_fn(_p: str, r: str, _step: int) -> str:
        return r + " better"

    t = SigmaTTT(gate=gate, trigger_threshold=0.4, max_steps=5)
    out = t.process("p", "x", adapt_fn=adapt_fn)
    assert out["path"] == "ADAPTED"
    assert out["σ_improvement"] == pytest.approx(0.9 - 0.1, abs=0.01)


def test_fast_weight_stored(monkeypatch: pytest.MonkeyPatch) -> None:
    gate = SigmaGate()

    def seq(_p: str, r: str) -> tuple[float, str]:
        return (0.1, "ACCEPT") if "adapted" in r else (0.9, "ABSTAIN")

    monkeypatch.setattr(gate, "score", seq)
    t = SigmaTTT(gate=gate, trigger_threshold=0.5)
    t.process("hello world prompt", "x")
    assert t.has_fast_weight("hello world prompt")


def test_fast_weight_recalled(monkeypatch: pytest.MonkeyPatch) -> None:
    gate = SigmaGate()
    monkeypatch.setattr(gate, "score", lambda _p, r: (0.05, "ACCEPT") if "step" in r else (0.8, "RETHINK"))
    t = SigmaTTT(gate=gate, trigger_threshold=0.4)
    t.process("recall key test", "init")
    got = t.recall_fast_weight("recall key test")
    assert got is not None
    assert "steps" in got


def test_efficiency_skip_rate(monkeypatch: pytest.MonkeyPatch) -> None:
    gate = SigmaGate()
    monkeypatch.setattr(gate, "score", lambda _p, _r: (0.01, "ACCEPT"))
    t = SigmaTTT(gate=gate, trigger_threshold=0.5, max_steps=7)
    for _ in range(3):
        t.process("a", "b")
    eff = t.efficiency()
    assert eff["skips"] == 3
    assert eff["skip_rate"] == pytest.approx(1.0)
    assert eff["compute_saved_steps"] == 3 * 7


def test_partial_adaptation(monkeypatch: pytest.MonkeyPatch) -> None:
    gate = SigmaGate()
    monkeypatch.setattr(gate, "score", lambda _p, _r: (0.85, "ABSTAIN"))
    t = SigmaTTT(gate=gate, trigger_threshold=0.2, max_steps=3)
    out = t.process("p", "r")
    assert out["path"] == "ADAPTED_PARTIAL"
    assert out["steps"] == 3
    assert out["verdict"] in ("RETHINK", "ABSTAIN")
