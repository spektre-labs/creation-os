# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""v144 σ-health + σ-watchdog lab: rules, remediation hooks, CLI-free run_once."""
from __future__ import annotations

from typing import Any, Dict

from cos.sigma_health import LabHealthGate, SigmaHealth
from cos.sigma_watchdog import SigmaWatchdog


class _CriticalMemHealth(SigmaHealth):
    def system_health(self) -> Dict[str, Any]:
        return {
            "cpu": {"percent": 10.0, "status": "ok"},
            "memory": {"percent": 96.0, "available_mb": 100, "status": "critical"},
            "disk": {"percent": 50.0, "free_gb": 10, "status": "ok"},
        }


def test_watchdog_high_sigma_triggers_reset() -> None:
    gate = LabHealthGate(avg_sigma=0.95, abstain_rate=0.01, avg_latency_ms=1.0)
    health = SigmaHealth(gate, thresholds={"sigma_avg": 0.6})
    wd = SigmaWatchdog(health, interval=1.0)
    wd.run_once()
    assert gate.reset_calls >= 1
    assert float(gate.get_stats()["avg_sigma"]) < 0.95


def test_watchdog_memory_critical_prunes() -> None:
    gate = LabHealthGate()
    health = _CriticalMemHealth(gate)
    wd = SigmaWatchdog(health, interval=1.0)
    wd.run_once()
    assert gate.prune_calls >= 1


def test_watchdog_model_reload_after_failure() -> None:
    gate = LabHealthGate()
    gate.model.break_()
    health = SigmaHealth(gate)
    wd = SigmaWatchdog(health, interval=1.0)
    wd.run_once()
    assert gate.model.reload_calls >= 1
    st = health.check()
    assert st["model"].get("status") == "ok"


def test_watchdog_sigma_drift_checkpoint() -> None:
    gate = LabHealthGate(avg_sigma=0.1)
    health = SigmaHealth(gate, thresholds={"sigma_avg": 0.99})
    for i in range(9):
        gate.set_sigma(0.1 + 0.01 * i)
        health.check()
    gate.set_sigma(0.1 + 0.01 * 9)
    wd = SigmaWatchdog(health, interval=1.0)
    wd.run_once()
    assert gate.model.checkpoint_calls >= 1
