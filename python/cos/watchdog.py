# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Health watchdog: σ-gate probes for **semantic** liveness (not only PID alive).

Scores a fixed prompt/answer pair on each check. Low σ + sub-second latency ⇒ healthy.
Accumulate consecutive unhealthy probes; after a cap, signal restart (via callback or
:meth:`needs_restart`).
"""
from __future__ import annotations

import threading
import time
from typing import Any, Callable, Dict, List, Optional

from cos.sigma_gate import SigmaGate

OnUnhealthy = Optional[Callable[[Dict[str, Any]], None]]


class SigmaWatchdog:
    """Self-healing watchdog: σ-based health monitoring."""

    _max_latency_ms: float = 5000.0

    def __init__(
        self,
        gate: Any = None,
        check_interval: float = 30.0,
        *,
        fabric: Any = None,
        sigma_threshold: float = 0.8,
        max_consecutive_failures: int = 3,
    ) -> None:
        self.gate = gate or SigmaGate()
        self.fabric = fabric
        self.check_interval = float(check_interval)
        self.sigma_threshold = float(sigma_threshold)
        self.σ_threshold = self.sigma_threshold  # noqa: PLC2401 — public σ alias
        self.max_failures = int(max_consecutive_failures)
        self.consecutive_failures = 0
        self.running = False
        self.health_log: List[Dict[str, Any]] = []
        self._sigma_samples: List[float] = []
        self._thread: Optional[threading.Thread] = None
        self._lock = threading.Lock()

    def record_sigma(self, sigma: float) -> None:
        """Record a pipeline σ from :class:`~cos.fabric.Fabric` for diagnostics (optional)."""
        with self._lock:
            self._sigma_samples.append(float(sigma))
            if len(self._sigma_samples) > 4096:
                self._sigma_samples = self._sigma_samples[-4096:]

    def start_background(self, on_unhealthy: OnUnhealthy = None) -> None:
        """Alias for :meth:`start` used by Fabric when ``COS_WATCHDOG_AUTO_START=1``."""
        self.start(on_unhealthy=on_unhealthy)

    def health_check(self) -> Dict[str, Any]:
        """Run one probe: σ-gate responsive and σ below threshold, latency under cap."""
        try:
            t0 = time.perf_counter()
            sigma, verdict = self.gate.score("watchdog health check", "system ok")
            latency_ms = (time.perf_counter() - t0) * 1000.0
            sigma_f = float(sigma)
            healthy = latency_ms < self._max_latency_ms and sigma_f < self.sigma_threshold

            result: Dict[str, Any] = {
                "timestamp": time.time(),
                "healthy": healthy,
                "sigma": round(sigma_f, 4),
                "σ": round(sigma_f, 4),
                "latency_ms": round(latency_ms, 1),
                "verdict": str(verdict),
            }
            with self._lock:
                self.health_log.append(result)
                if healthy:
                    self.consecutive_failures = 0
                else:
                    self.consecutive_failures += 1
            return result
        except Exception as e:  # noqa: BLE001 — watchdog must never crash the probe path
            with self._lock:
                self.consecutive_failures += 1
                result = {
                    "timestamp": time.time(),
                    "healthy": False,
                    "error": str(e),
                }
                self.health_log.append(result)
            return result

    def needs_restart(self) -> bool:
        """True when unhealthy streak reached ``max_consecutive_failures``."""
        with self._lock:
            return self.consecutive_failures >= self.max_failures

    def start(self, on_unhealthy: OnUnhealthy = None) -> None:
        """Start background health monitoring (daemon thread)."""
        self.running = True

        def monitor() -> None:
            while self.running:
                result = self.health_check()
                if not result.get("healthy", False) and on_unhealthy is not None:
                    on_unhealthy(result)
                with self._lock:
                    need_restart = self.consecutive_failures >= self.max_failures
                    failures_for_cb = self.consecutive_failures
                if need_restart:
                    if on_unhealthy is not None:
                        on_unhealthy(
                            {
                                "action": "RESTART_REQUIRED",
                                "failures": failures_for_cb,
                            }
                        )
                    with self._lock:
                        self.consecutive_failures = 0
                time.sleep(self.check_interval)

        self._thread = threading.Thread(target=monitor, daemon=True)
        self._thread.start()

    def stop(self) -> None:
        """Request monitor loop exit (next iteration observes ``running`` clear)."""
        self.running = False

    def status(self) -> Dict[str, Any]:
        with self._lock:
            n = len(self.health_log)
            ok = sum(1 for h in self.health_log if h.get("healthy") is True)
            nf = self.consecutive_failures
            run = self.running
            nr = nf >= self.max_failures
        return {
            "running": run,
            "consecutive_failures": nf,
            "total_checks": n,
            "healthy_rate": round(ok / max(n, 1), 4),
            "needs_restart": nr,
        }


__all__ = ["SigmaWatchdog"]
