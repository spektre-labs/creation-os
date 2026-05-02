# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
"""v150 σ-TTT v2: gate-guided skip/apply, early stop, adaptive budget."""

from __future__ import annotations

import time

from cos.sigma_ttt_v2 import SigmaTTTv2, TTTEarlyStopStubModel, TTTLabStubModel


class _GateEasy:
    def score(self, prompt: str, response: str):
        del prompt
        if response.strip() == "4":
            return 0.04, "ACCEPT"
        return 0.5, "RETHINK"


class _GateImproving:
    def score(self, prompt: str, response: str):
        del prompt
        r = response
        if "UNCERTAIN" in r:
            return 0.55, "RETHINK"
        if "REFINED" in r or "proof sketch" in r:
            return 0.05, "ACCEPT"
        return 0.35, "RETHINK"


class _GateStuckHigh:
    def score(self, prompt: str, response: str):
        del prompt, response
        return 0.65, "RETHINK"


class _SlowStub(TTTLabStubModel):
    def generate(self, prompt: str, *, temperature: float = 0.0) -> str:
        time.sleep(0.012)
        return super().generate(prompt, temperature=temperature)


def test_easy_query_zero_ttt_steps() -> None:
    eng = SigmaTTTv2(TTTLabStubModel(), _GateEasy(), max_steps=5)
    out = eng.inference(prompt="What is 2+2?")
    assert out["ttt_steps"] == 0
    assert out.get("reason") == "low σ, TTT not needed"
    assert int(eng.stats["ttt_skipped"]) == 1


def test_hard_query_ttt_reduces_sigma() -> None:
    eng = SigmaTTTv2(TTTLabStubModel(), _GateImproving(), max_steps=10)
    out = eng.inference(prompt="Prove that sqrt(2) is irrational.")
    assert int(out["ttt_steps"]) >= 1
    assert float(out["sigma"]) < 0.2
    assert "REFINED" in str(out["response"]) or "proof" in str(out["response"]).lower()
    assert int(eng.stats["ttt_applied"]) == 1


def test_ttt_early_stop_when_no_sigma_improvement() -> None:
    eng = SigmaTTTv2(TTTEarlyStopStubModel(), _GateStuckHigh(), max_steps=10)
    out = eng.inference(prompt="any prompt")
    assert 3 <= int(out["ttt_steps"]) <= 4
    assert int(eng.stats["ttt_applied"]) == 1


def test_adaptive_compute_respects_budget() -> None:
    eng = SigmaTTTv2(_SlowStub(), _GateImproving(), max_steps=3)
    out = eng.adaptive_compute(prompt="Explain P vs NP briefly.", budget_ms=120.0)
    assert float(out["elapsed_ms"]) <= float(out["budget_ms"]) + 80.0
    assert float(out["sigma"]) < 0.2
