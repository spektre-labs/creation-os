# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.retry`."""
from __future__ import annotations

from cos.retry import SigmaRetry
from cos.sigma_gate import SigmaGate


def test_backoff_increases() -> None:
    assert SigmaRetry.backoff(2) < SigmaRetry.backoff(5)


def test_circuit_breaker_trips() -> None:
    r = SigmaRetry.circuit_breaker(3, 3)
    assert r["open"] is True


def test_budget_aware() -> None:
    assert SigmaRetry.budget_aware(1.0, 0.5)["can_retry"] is True
    assert SigmaRetry.budget_aware(0.1, 0.5)["can_retry"] is False


def test_sigma_trend() -> None:
    t = SigmaRetry.sigma_trend([{"sigma": 0.8}, {"sigma": 0.3}])
    assert t["improving"] is True


def test_retry_rephrase_eventually_accepts() -> None:
    gate = SigmaGate()
    calls = {"n": 0}

    def fn(ctx: dict) -> dict:
        calls["n"] += 1
        return {
            "prompt": "What is 2+2?",
            "response": "4" if calls["n"] >= 2 else "pong",
        }

    out = SigmaRetry.retry(fn, gate, 4, "rephrase", {"prompt": "What is 2+2?", "response": "pong"})
    assert str(out["attempts"][-1]["verdict"]).upper() == "ACCEPT"


def test_apply_strategy_invalid() -> None:
    try:
        SigmaRetry._apply_strategy("nope", lambda x: x, {}, attempt=1)
    except ValueError:
        return
    raise AssertionError("expected ValueError")
