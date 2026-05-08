# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.watchdog` (σ-based health probes)."""
from __future__ import annotations

from typing import Tuple

from cos.watchdog import SigmaWatchdog


class _FakeGateGood:
    def score(self, prompt: str, response: str) -> Tuple[float, str]:
        del prompt, response
        return (0.1, "ACCEPT")


class _FakeGateBad:
    def score(self, prompt: str, response: str) -> Tuple[float, str]:
        del prompt, response
        return (0.95, "ABSTAIN")


def test_health_check_healthy() -> None:
    wd = SigmaWatchdog(gate=_FakeGateGood(), sigma_threshold=0.8)
    r = wd.health_check()
    assert r["healthy"] is True
    assert r["sigma"] < wd.sigma_threshold
    assert r["sigma"] == r["σ"]
    assert r["latency_ms"] < 5000.0
    assert wd.consecutive_failures == 0


def test_consecutive_failures_tracked() -> None:
    wd = SigmaWatchdog(gate=_FakeGateBad(), max_consecutive_failures=5, sigma_threshold=0.8)
    wd.health_check()
    assert wd.consecutive_failures == 1
    wd.health_check()
    assert wd.consecutive_failures == 2


def test_needs_restart_after_max_failures() -> None:
    wd = SigmaWatchdog(gate=_FakeGateBad(), max_consecutive_failures=3, sigma_threshold=0.8)
    for _ in range(2):
        wd.health_check()
        assert wd.needs_restart() is False
    wd.health_check()
    assert wd.needs_restart() is True


def test_status_report() -> None:
    wd = SigmaWatchdog(gate=_FakeGateGood(), max_consecutive_failures=3, sigma_threshold=0.8)
    wd.health_check()
    wd.health_check()
    st = wd.status()
    assert st["total_checks"] == 2
    assert st["healthy_rate"] == 1.0
    assert st["consecutive_failures"] == 0
    assert st["needs_restart"] is False


def test_healthy_resets_counter() -> None:
    wd = SigmaWatchdog(gate=_FakeGateBad(), max_consecutive_failures=3, sigma_threshold=0.8)
    wd.health_check()
    wd.health_check()
    assert wd.consecutive_failures == 2
    wd.gate = _FakeGateGood()  # type: ignore[assignment]
    wd.health_check()
    assert wd.consecutive_failures == 0
    assert wd.needs_restart() is False
