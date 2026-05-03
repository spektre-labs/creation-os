# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
σ-health — runtime posture for **system**, **σ-gate statistics**, and **model** probes.

Health inspects the stack; observation (``cos observe``) inspects outputs. This module
does **not** change ``sigma_gate.h``. Optional ``psutil`` improves CPU/RAM/disk metrics
(``pip install 'creation-os[health]'``).
"""
from __future__ import annotations

import os
import time
from typing import Any, Callable, Dict, List, Optional


def _disk_root() -> str:
    if os.name == "nt":
        return os.environ.get("SystemRoot", r"C:\Windows")[:2] + "\\"
    return "/"


def _rank(s: str) -> int:
    return {"ok": 0, "unknown": 1, "warning": 2, "critical": 3}.get(s, 0)


def _max_status_in_tree(obj: Any) -> str:
    """Return worst status string found under nested dicts."""
    worst = "ok"
    wr = 0
    if isinstance(obj, dict):
        st = obj.get("status")
        if isinstance(st, str):
            r = _rank(st)
            if r > wr:
                wr = r
                worst = st
        for v in obj.values():
            cand = _max_status_in_tree(v)
            if _rank(cand) > wr:
                wr = _rank(cand)
                worst = cand
    return worst


class LabHealthGate:
    """
    Minimal gate surface for lab tests and ``cos health --runtime``.

    Production wiring can replace this with an object exposing the same optional methods.
    """

    def __init__(
        self,
        *,
        avg_sigma: float = 0.11,
        abstain_rate: float = 0.05,
        avg_latency_ms: float = 0.8,
    ) -> None:
        self._avg_sigma = float(avg_sigma)
        self._abstain_rate = float(abstain_rate)
        self._avg_latency_ms = float(avg_latency_ms)
        self.reset_calls = 0
        self.prune_calls = 0
        self.calibrate_calls = 0
        self.model = _LabModel()

    def get_stats(self) -> Dict[str, Any]:
        return {
            "avg_sigma": float(self._avg_sigma),
            "abstain_rate": float(self._abstain_rate),
            "avg_latency_ms": float(self._avg_latency_ms),
        }

    def reset_state(self) -> None:
        self.reset_calls += 1
        self._avg_sigma = max(0.0, float(self._avg_sigma) * 0.5)

    def prune_kv_cache(self, threshold: float = 0.5) -> None:
        _ = threshold
        self.prune_calls += 1

    def auto_calibrate(self) -> None:
        self.calibrate_calls += 1
        self._abstain_rate = max(0.0, float(self._abstain_rate) * 0.5)

    @property
    def engram(self) -> "_LabEngram":
        return _LabEngram()

    def set_sigma(self, v: float) -> None:
        self._avg_sigma = float(v)

    def set_abstain_rate(self, v: float) -> None:
        self._abstain_rate = float(v)


class _LabEngram:
    def forget(self, max_sigma: float = 0.8) -> None:
        _ = max_sigma


class _LabModel:
    def __init__(self) -> None:
        self._broken = False
        self.reload_calls = 0
        self.checkpoint_calls = 0

    def generate(self, prompt: str, *, max_tokens: int = 5) -> str:
        _ = max_tokens
        if self._broken:
            raise RuntimeError("model_unresponsive_lab")
        return f"ok:{prompt[:32]}"

    def reload(self) -> None:
        self.reload_calls += 1
        self._broken = False

    def save_checkpoint(self) -> None:
        self.checkpoint_calls += 1

    def break_(self) -> None:
        self._broken = True


class SigmaHealth:
    """Rolling σ-runtime health: system metrics (optional psutil) + gate stats + model ping."""

    def __init__(self, gate: Any, *, thresholds: Optional[Dict[str, float]] = None) -> None:
        self.gate = gate
        self.history: List[Dict[str, Any]] = []
        self.thresholds: Dict[str, float] = {
            "cpu_percent": 90.0,
            "memory_percent": 85.0,
            "disk_percent": 95.0,
            "sigma_avg": 0.6,
            "abstain_rate": 0.3,
            "gate_latency_ms": 100.0,
        }
        if thresholds:
            self.thresholds.update(thresholds)

    def system_health(self) -> Dict[str, Any]:
        try:
            import psutil  # type: ignore[import-untyped]
        except ImportError:
            return {
                "note": "psutil not installed — install creation-os[health] for CPU/RAM/disk metrics",
                "cpu": {"percent": None, "status": "unknown"},
                "memory": {"percent": None, "available_mb": None, "status": "unknown"},
                "disk": {"percent": None, "free_gb": None, "status": "unknown"},
            }

        cpu = float(psutil.cpu_percent(interval=0.25))
        mem = psutil.virtual_memory()
        disk = psutil.disk_usage(_disk_root())
        return {
            "cpu": {
                "percent": cpu,
                "status": "critical" if cpu > 95 else "warning" if cpu > self.thresholds["cpu_percent"] else "ok",
            },
            "memory": {
                "percent": float(mem.percent),
                "available_mb": int(mem.available // (1024 * 1024)),
                "status": (
                    "critical"
                    if mem.percent > 95
                    else "warning"
                    if mem.percent > self.thresholds["memory_percent"]
                    else "ok"
                ),
            },
            "disk": {
                "percent": float(disk.percent),
                "free_gb": int(disk.free // (1024 * 1024 * 1024)),
                "status": (
                    "critical"
                    if disk.percent > 99
                    else "warning"
                    if disk.percent > self.thresholds["disk_percent"]
                    else "ok"
                ),
            },
        }

    def sigma_health(self) -> Dict[str, Any]:
        stats_fn: Optional[Callable[[], Dict[str, Any]]] = getattr(self.gate, "get_stats", None)
        if not callable(stats_fn):
            return {
                "avg_sigma": {"value": None, "status": "unknown"},
                "abstain_rate": {"value": None, "status": "unknown"},
                "gate_latency_ms": {"value": None, "status": "unknown"},
            }
        stats = stats_fn()
        avg_s = float(stats.get("avg_sigma", 0.0))
        abst = float(stats.get("abstain_rate", 0.0))
        lat = float(stats.get("avg_latency_ms", 0.0))
        return {
            "avg_sigma": {
                "value": avg_s,
                "status": "warning" if avg_s > self.thresholds["sigma_avg"] else "ok",
            },
            "abstain_rate": {
                "value": abst,
                "status": "warning" if abst > self.thresholds["abstain_rate"] else "ok",
            },
            "gate_latency_ms": {
                "value": lat,
                "status": "warning" if lat > self.thresholds["gate_latency_ms"] else "ok",
            },
        }

    def model_health(self) -> Dict[str, Any]:
        model = getattr(self.gate, "model", None)
        gen = getattr(model, "generate", None) if model is not None else None
        if not callable(gen):
            return {
                "loaded": False,
                "responsive": False,
                "status": "unknown",
                "error": "gate.model.generate not available",
            }
        try:
            t0 = time.monotonic()
            try:
                out = gen("health_ping", max_tokens=5)  # type: ignore[misc]
            except TypeError:
                out = gen("health_ping", 5)  # type: ignore[misc, call-arg]
            _ = out
            latency_ms = (time.monotonic() - t0) * 1000.0
            return {
                "loaded": True,
                "responsive": True,
                "latency_ms": round(latency_ms, 2),
                "status": "ok",
            }
        except Exception as e:
            return {
                "loaded": False,
                "responsive": False,
                "error": str(e),
                "status": "critical",
            }

    def check(self) -> Dict[str, Any]:
        status: Dict[str, Any] = {
            "timestamp": time.time(),
            "system": self.system_health(),
            "sigma": self.sigma_health(),
            "model": self.model_health(),
            "overall": "healthy",
        }
        sys_w = _max_status_in_tree(status["system"])
        sig_w = _max_status_in_tree(status["sigma"])
        mod_w = _max_status_in_tree(status["model"])
        worst = max(_rank(sys_w), _rank(sig_w), _rank(mod_w))
        if worst >= _rank("critical"):
            status["overall"] = "critical"
        elif worst >= _rank("warning"):
            status["overall"] = "degraded"
        else:
            status["overall"] = "healthy"
        self.history.append(status)
        return status


__all__ = ["SigmaHealth", "LabHealthGate"]
