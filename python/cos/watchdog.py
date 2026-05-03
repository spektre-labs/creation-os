# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-watchdog — health snapshots, drift heuristics, optional background heartbeat.

Rollback hooks are **advisory**; wire to :mod:`cos.fabric` snapshots in production hosts.
See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import threading
import time
from collections import deque
from typing import Any, Deque, Dict, List, Optional

__all__ = ["SigmaWatchdog"]


class SigmaWatchdog:
    """Monitor gate, σ stream, and coarse resources (psutil optional)."""

    def __init__(
        self,
        gate: Any = None,
        *,
        fabric: Any = None,
    ) -> None:
        from cos.sigma_gate import SigmaGate

        self.gate = gate or SigmaGate()
        self.fabric = fabric
        self._last_heartbeat = time.monotonic()
        self._sigma_window: Deque[tuple[float, float]] = deque(maxlen=4096)
        self._baseline_mean: Optional[float] = None
        self.alerts: List[Dict[str, Any]] = []
        self._stop = threading.Event()
        self._thread: Optional[threading.Thread] = None

    def attach_fabric(self, fabric: Any) -> None:
        self.fabric = fabric

    def heartbeat(self) -> Dict[str, Any]:
        self._last_heartbeat = time.monotonic()
        return {"ts": self._last_heartbeat, "ok": True}

    def deadman_expired(self, timeout_seconds: float = 120.0) -> bool:
        return (time.monotonic() - self._last_heartbeat) > float(timeout_seconds)

    def health_check(self) -> Dict[str, Any]:
        gate_ok = bool(self.gate is not None)
        mem_ok = True
        try:
            import resource

            usage = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        except ImportError:
            usage = 0
        return {
            "gate_ok": gate_ok,
            "avg_gate_sigma": round(float(getattr(self.gate, "avg_sigma", 0.0)), 6),
            "max_rss_kb_hint": int(usage),
            "memory_ok": mem_ok,
            "probes_ok": True,
            "note": "Lab snapshot; extend with LSD pickle checks in full trees.",
        }

    def record_sigma(self, sigma: float) -> None:
        self._sigma_window.append((time.monotonic(), float(sigma)))

    def set_baseline_from_samples(self, samples: List[float]) -> None:
        if not samples:
            return
        self._baseline_mean = sum(float(s) for s in samples) / len(samples)

    def sigma_drift_monitor(self, window_minutes: float = 15.0) -> Dict[str, Any]:
        now = time.monotonic()
        horizon = float(window_minutes) * 60.0
        recent = [s for t, s in self._sigma_window if now - t <= horizon]
        if not recent or self._baseline_mean is None:
            return {"drift": 0.0, "baseline": self._baseline_mean, "n": len(recent)}
        cur = sum(recent) / len(recent)
        drift = float(cur - float(self._baseline_mean))
        return {
            "drift": round(drift, 6),
            "current_mean": round(cur, 6),
            "baseline": round(float(self._baseline_mean), 6),
            "n": len(recent),
        }

    def auto_alert(self, drift_report: Dict[str, Any], *, threshold: float = 0.15) -> bool:
        drift = float(drift_report.get("drift", 0.0))
        if abs(drift) > threshold:
            alert = {"kind": "sigma_drift", "drift": drift, "ts": time.time()}
            self.alerts.append(alert)
            return True
        return False

    def auto_rollback(
        self,
        drift_report: Dict[str, Any],
        *,
        critical: float = 0.35,
    ) -> Dict[str, Any]:
        drift = abs(float(drift_report.get("drift", 0.0)))
        would = drift > float(critical)
        out: Dict[str, Any] = {"would_rollback": bool(would), "drift": drift}
        if would and self.fabric is not None:
            mgr = getattr(self.fabric, "rollback", None)
            if callable(mgr):
                try:
                    out["rollback"] = mgr(None)
                except (TypeError, OSError, RuntimeError) as exc:
                    out["rollback_error"] = str(exc)
        return out

    def resource_monitor(self) -> Dict[str, Any]:
        cpu_pct = 0.0
        ram_pct = 0.0
        disk_pct = 0.0
        gpu_pct: Optional[float] = None
        try:
            import psutil  # type: ignore[import-not-found]

            cpu_pct = float(psutil.cpu_percent(interval=None))
            ram_pct = float(psutil.virtual_memory().percent)
            disk_pct = float(psutil.disk_usage("/").percent)
        except ImportError:
            try:
                import os

                load = os.getloadavg()[0]
                cpu_pct = min(100.0, 100.0 * float(load) / 8.0)
            except (AttributeError, OSError):
                cpu_pct = 0.0
        return {
            "cpu_percent": round(cpu_pct, 2),
            "ram_percent": round(ram_pct, 2),
            "disk_percent": round(disk_pct, 2),
            "gpu_percent": gpu_pct,
            "note": "psutil optional; loadavg fallback on Unix.",
        }

    def start_background(self, interval_seconds: float = 30.0) -> None:
        if self._thread is not None and self._thread.is_alive():
            return

        def _loop() -> None:
            while not self._stop.wait(timeout=float(interval_seconds)):
                self.heartbeat()
                self.health_check()

        self._thread = threading.Thread(target=_loop, daemon=True)
        self._thread.start()

    def stop_background(self) -> None:
        self._stop.set()
        if self._thread is not None:
            self._thread.join(timeout=2.0)
            self._thread = None
        self._stop.clear()

