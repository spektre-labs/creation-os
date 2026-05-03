# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
σ-watchdog — Ω-loop phase **WATCHDOG** (lab): rule-driven remediation hooks on :class:`SigmaHealth`.

This is a **process-local** monitor (thread); it is not a kernel watchdog timer and does not
replace host-level supervision. Does not modify ``sigma_gate.h``.
"""
from __future__ import annotations

import threading
import time
from typing import Any, Callable, Dict, List, Optional

from cos.sigma_health import SigmaHealth

RuleFn = Callable[[Dict[str, Any]], Optional[Dict[str, Any]]]


class SigmaWatchdog:
    """Evaluate remediation rules against the latest :meth:`SigmaHealth.check` snapshot."""

    def __init__(self, health: SigmaHealth, *, interval: float = 30.0) -> None:
        self.health = health
        self.interval = float(interval)
        self.running = False
        self._thread: Optional[threading.Thread] = None
        self.actions_taken: List[Dict[str, Any]] = []
        self.event_log: List[Dict[str, Any]] = []
        self.checks: int = 0
        self.remediation_rules: List[RuleFn] = [
            self.rule_high_sigma,
            self.rule_high_memory,
            self.rule_model_unresponsive,
            self.rule_abstain_spike,
            self.rule_sigma_drift,
        ]

    def start(self) -> None:
        self.running = True
        self._thread = threading.Thread(target=self._loop, daemon=True, name="sigma-watchdog")
        self._thread.start()

    def stop(self) -> None:
        self.running = False

    def join(self, timeout: Optional[float] = None) -> None:
        if self._thread is not None:
            self._thread.join(timeout=timeout)

    def _loop(self) -> None:
        while self.running:
            self.run_once()
            time.sleep(self.interval)

    def run_once(self) -> Dict[str, Any]:
        """Perform a single check + rule pass (used by CLI tests and --once)."""
        status = self.health.check()
        self.checks += 1
        self.event_log.append(
            {"timestamp": time.time(), "kind": "check", "overall": status.get("overall"), "actions": []}
        )
        actions_this_round: List[str] = []
        for rule in self.remediation_rules:
            action = rule(status)
            if action:
                self.execute(action)
                actions_this_round.append(str(action.get("action", "")))
        self.event_log[-1]["actions"] = actions_this_round
        return status

    def rule_high_sigma(self, status: Dict[str, Any]) -> Optional[Dict[str, Any]]:
        sigma = status.get("sigma", {}).get("avg_sigma", {})
        if isinstance(sigma, dict) and sigma.get("status") == "warning":
            return {"rule": "high_sigma", "action": "reset_gate_state", "severity": "warning"}
        return None

    def rule_high_memory(self, status: Dict[str, Any]) -> Optional[Dict[str, Any]]:
        mem = status.get("system", {}).get("memory", {})
        if isinstance(mem, dict) and mem.get("status") == "critical":
            return {"rule": "high_memory", "action": "prune_caches", "severity": "critical"}
        return None

    def rule_model_unresponsive(self, status: Dict[str, Any]) -> Optional[Dict[str, Any]]:
        model = status.get("model", {})
        if isinstance(model, dict) and model.get("status") == "critical":
            return {"rule": "model_unresponsive", "action": "restart_model", "severity": "critical"}
        return None

    def rule_abstain_spike(self, status: Dict[str, Any]) -> Optional[Dict[str, Any]]:
        abstain = status.get("sigma", {}).get("abstain_rate", {})
        if isinstance(abstain, dict) and abstain.get("status") == "warning":
            return {"rule": "abstain_spike", "action": "recalibrate_thresholds", "severity": "warning"}
        return None

    def rule_sigma_drift(self, status: Dict[str, Any]) -> Optional[Dict[str, Any]]:
        _ = status
        if len(self.health.history) < 10:
            return None
        recent = [
            float(h.get("sigma", {}).get("avg_sigma", {}).get("value", 0.0))
            for h in self.health.history[-10:]
        ]
        if len(recent) < 2:
            return None
        if all(recent[i] <= recent[i + 1] for i in range(len(recent) - 1)) and recent[-1] > recent[0]:
            return {"rule": "sigma_drift", "action": "alert_and_checkpoint", "severity": "warning"}
        return None

    def execute(self, action: Dict[str, Any]) -> None:
        gate = self.health.gate
        rec = {**action, "timestamp": time.time(), "executed": True}
        self.actions_taken.append(rec)
        self.event_log.append({"timestamp": time.time(), "kind": "action", **action})

        name = str(action.get("action", ""))
        if name == "reset_gate_state":
            rs = getattr(gate, "reset_state", None)
            if callable(rs):
                rs()
        elif name == "prune_caches":
            pv = getattr(gate, "prune_kv_cache", None)
            if callable(pv):
                pv(0.5)
            eg = getattr(gate, "engram", None)
            forget = getattr(eg, "forget", None) if eg is not None else None
            if callable(forget):
                forget(max_sigma=0.8)
        elif name == "restart_model":
            model = getattr(gate, "model", None)
            rel = getattr(model, "reload", None) if model is not None else None
            if callable(rel):
                rel()
        elif name == "recalibrate_thresholds":
            ac = getattr(gate, "auto_calibrate", None)
            if callable(ac):
                ac()
        elif name == "alert_and_checkpoint":
            model = getattr(gate, "model", None)
            ck = getattr(model, "save_checkpoint", None) if model is not None else None
            if callable(ck):
                ck()


_SERVICE_LOCK = threading.Lock()
_SERVICE: Optional[SigmaWatchdog] = None


def watchdog_service() -> Optional[SigmaWatchdog]:
    with _SERVICE_LOCK:
        return _SERVICE


def watchdog_bind(wd: Optional[SigmaWatchdog]) -> None:
    global _SERVICE
    with _SERVICE_LOCK:
        _SERVICE = wd


__all__ = ["SigmaWatchdog", "watchdog_bind", "watchdog_service"]
