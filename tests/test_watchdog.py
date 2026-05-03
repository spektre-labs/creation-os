# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import time

from cos.watchdog import SigmaWatchdog


def test_health_check_shape() -> None:
    w = SigmaWatchdog()
    h = w.health_check()
    assert h["gate_ok"] is True


def test_sigma_drift_monitor() -> None:
    w = SigmaWatchdog()
    w.set_baseline_from_samples([0.2, 0.22, 0.21])
    w.record_sigma(0.55)
    w.record_sigma(0.56)
    d = w.sigma_drift_monitor(window_minutes=60.0)
    assert "drift" in d


def test_auto_alert_triggers() -> None:
    w = SigmaWatchdog()
    fired = w.auto_alert({"drift": 0.5}, threshold=0.2)
    assert fired is True
    assert len(w.alerts) >= 1


def test_auto_rollback_flag() -> None:
    class _Fab:
        def rollback(self, _t: object) -> dict[str, bool]:
            return {"ok": True}

    w = SigmaWatchdog(fabric=_Fab())
    out = w.auto_rollback({"drift": 0.9}, critical=0.3)
    assert out["would_rollback"] is True


def test_heartbeat_deadman() -> None:
    w = SigmaWatchdog()
    w.heartbeat()
    assert w.deadman_expired(timeout_seconds=3600.0) is False


def test_resource_monitor_keys() -> None:
    w = SigmaWatchdog()
    r = w.resource_monitor()
    assert "cpu_percent" in r


def test_background_start_stop() -> None:
    w = SigmaWatchdog()
    w.start_background(interval_seconds=0.05)
    time.sleep(0.12)
    w.stop_background()
