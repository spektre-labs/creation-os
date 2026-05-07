# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Production observability: trace σ-gate calls, detect drift signals, alert on regression.

σ is the sole quality scalar — no parallel “faithfulness” metrics. OpenTelemetry export
is optional via :func:`cos.sigma_observe.post_otlp_http` (no hard dependency on an SDK).
"""
from __future__ import annotations

import json
import statistics
import time
import uuid
from collections import Counter, deque
from pathlib import Path
from typing import Any, Deque, Dict, List, Optional

__all__ = ["SigmaObserve"]


class SigmaObserve:
    """Lightweight σ observability — JSONL logs under ``~/.cos/logs``, rolling window in memory."""

    def __init__(
        self,
        log_dir: str = "~/.cos/logs",
        window_size: int = 1000,
        *,
        otel_endpoint: Optional[str] = None,
    ) -> None:
        self.log_dir = Path(log_dir).expanduser()
        self.log_dir.mkdir(parents=True, exist_ok=True)
        self.window: Deque[Dict[str, Any]] = deque(maxlen=max(1, int(window_size)))
        self.alerts: List[Dict[str, Any]] = []
        self._otel_endpoint = (otel_endpoint or "").strip() or None
        self._otel_failures = 0
        # Legacy fabric / lab hooks
        self._layer_sigmas: List[Dict[str, Any]] = []
        self._cost_units: float = 0.0
        self._latencies_ms: List[float] = []

    def set_otel_endpoint(self, url: Optional[str]) -> None:
        self._otel_endpoint = (url or "").strip() or None

    def _maybe_otel(self, entry: Dict[str, Any]) -> None:
        if not self._otel_endpoint:
            return
        try:
            from cos.sigma_observe import post_otlp_http

            trace_entry = {
                "trace_id": uuid.uuid4().hex,
                "span_id": uuid.uuid4().hex[:16],
                "timestamp": float(entry["timestamp"]),
                "sigma": float(entry["sigma"]),
                "verdict": str(entry["verdict"]),
                "gate_latency_ms": float(entry["latency_ms"]),
                "model": str(entry.get("model") or ""),
            }
            post_otlp_http(self._otel_endpoint, trace_entry)
        except Exception:
            self._otel_failures += 1

    def record(
        self,
        prompt: str,
        response: str,
        sigma: float,
        verdict: str,
        latency_ms: float,
        *,
        model: Optional[str] = None,
        endpoint: Optional[str] = None,
        tokens: Optional[Any] = None,
        cost: Optional[float] = None,
    ) -> Dict[str, Any]:
        """Record one σ-gate call."""
        vraw = str(verdict.name) if hasattr(verdict, "name") else str(verdict)
        entry = {
            "timestamp": time.time(),
            "sigma": round(float(sigma), 4),
            "verdict": vraw,
            "latency_ms": round(float(latency_ms), 1),
            "model": model,
            "endpoint": endpoint,
            "tokens": tokens,
            "cost": cost,
            "prompt_len": len(str(prompt)),
            "response_len": len(str(response)),
        }
        self.window.append(entry)
        self._check_alerts(entry)
        self._log_to_disk(entry)
        self._maybe_otel(entry)
        return entry

    def summary(self, last_n: Optional[int] = None) -> Dict[str, Any]:
        """Dashboard summary over the in-memory window (optionally last ``last_n`` rows)."""
        seq = list(self.window)
        if last_n is not None:
            seq = seq[-max(0, int(last_n)) :]
        if not seq:
            return {"count": 0}
        sigmas = [float(d["sigma"]) for d in seq]
        verdicts = [str(d["verdict"]) for d in seq]
        latencies = [float(d["latency_ms"]) for d in seq]
        sig_sorted = sorted(sigmas)
        lat_sorted = sorted(latencies)
        n_s = len(sig_sorted)
        n_l = len(lat_sorted)
        return {
            "count": len(seq),
            "σ_avg": round(sum(sigmas) / n_s, 4),
            "σ_p50": round(sig_sorted[n_s // 2], 4),
            "σ_p95": round(sig_sorted[int(len(sigmas) * 0.95)], 4),
            "σ_max": round(max(sigmas), 4),
            "accept_rate": round(verdicts.count("ACCEPT") / len(verdicts), 3),
            "rethink_rate": round(verdicts.count("RETHINK") / len(verdicts), 3),
            "abstain_rate": round(verdicts.count("ABSTAIN") / len(verdicts), 3),
            "latency_p50": round(lat_sorted[n_l // 2], 1),
            "latency_p95": round(lat_sorted[int(len(latencies) * 0.95)], 1),
            "total_cost": round(sum(float(d.get("cost") or 0) for d in seq), 4),
        }

    def _check_alerts(self, entry: Dict[str, Any]) -> None:
        if len(self.window) < 10:
            return
        recent_σ = [float(d["sigma"]) for d in list(self.window)[-10:]]
        avg = sum(recent_σ) / len(recent_σ)
        if avg > 0.5:
            self.alerts.append(
                {
                    "type": "high_σ_avg",
                    "value": round(avg, 4),
                    "timestamp": entry["timestamp"],
                    "message": f"Average σ over last 10 calls is {avg:.3f} — quality degrading",
                }
            )
        win = list(self.window)
        if (
            len(win) >= 2
            and str(entry["verdict"]) == "ABSTAIN"
            and str(win[-2]["verdict"]) == "ABSTAIN"
        ):
            self.alerts.append(
                {
                    "type": "consecutive_abstain",
                    "timestamp": entry["timestamp"],
                    "message": "Two consecutive ABSTAINs — check model health",
                }
            )

    def _log_to_disk(self, entry: Dict[str, Any]) -> None:
        date = time.strftime("%Y-%m-%d")
        log_file = self.log_dir / f"sigma_{date}.jsonl"
        line = json.dumps(entry, default=str) + "\n"
        with open(log_file, "a", encoding="utf-8") as f:
            f.write(line)

    def today_log_path(self) -> Path:
        return self.log_dir / f"sigma_{time.strftime('%Y-%m-%d')}.jsonl"

    def load_today_jsonl(self) -> int:
        """Append entries from today's JSONL into the window (for CLI dashboard)."""
        return self.load_jsonl_file(self.today_log_path())

    def load_jsonl_file(self, path: Path) -> int:
        """Append valid JSONL rows into the in-memory window."""
        p = Path(path).expanduser()
        if not p.is_file():
            return 0
        n = 0
        with open(p, encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                try:
                    d = json.loads(line)
                except json.JSONDecodeError:
                    continue
                if isinstance(d, dict) and "sigma" in d and "verdict" in d:
                    self.window.append(d)
                    n += 1
        return n

    # --- Legacy API (fabric, older tests) ---------------------------------

    def trace_request(
        self,
        prompt: str,
        response: Optional[str],
        sigma: float,
        verdict: str,
        *,
        trace_name: str = "cos.request",
    ) -> Dict[str, Any]:
        """Append a span-shaped record and mirror into :meth:`record` (zero latency if unknown)."""
        self.record(
            str(prompt),
            str(response or ""),
            float(sigma),
            str(verdict),
            0.0,
            model=None,
            endpoint=None,
            tokens=None,
            cost=None,
        )
        return {
            "name": trace_name,
            "start_ns": time.time_ns(),
            "attributes": {
                "cos.prompt_len": len(str(prompt)),
                "cos.response_len": len(str(response or "")),
                "cos.sigma": round(float(sigma), 6),
                "cos.verdict": str(verdict),
            },
        }

    def sigma_histogram(self) -> Dict[str, Any]:
        sigs = [float(d["sigma"]) for d in self.window]
        if not sigs:
            return {"bins": [], "counts": [], "n": 0}
        bins = [0.0, 0.25, 0.5, 0.75, 1.0]
        counts = [0, 0, 0, 0]
        for s in sigs:
            for i in range(len(bins) - 1):
                if bins[i] <= s < bins[i + 1] or (i == len(bins) - 2 and s == 1.0):
                    counts[i] += 1
                    break
        return {"bins": bins, "counts": counts, "n": len(sigs)}

    def alert_on_drift(
        self,
        baseline_sigma: float,
        current_sigma: float,
        threshold: float,
    ) -> Dict[str, Any]:
        delta = abs(float(current_sigma) - float(baseline_sigma))
        return {
            "alert": bool(delta > float(threshold)),
            "delta": round(delta, 6),
            "baseline": float(baseline_sigma),
            "current": float(current_sigma),
        }

    def dashboard_data(self) -> Dict[str, Any]:
        summ = self.summary()
        n = int(summ.get("count", 0))
        verdict_counts: Counter[str] = Counter(str(d["verdict"]) for d in self.window)
        if n == 0:
            p95 = 0.0
            avg = 0.0
        else:
            p95 = float(summ["σ_p95"])
            avg = float(summ["σ_avg"])
        lat_extra = list(self._latencies_ms)
        lat_from_window = [float(d["latency_ms"]) for d in self.window]
        lat_all = lat_from_window + lat_extra
        return {
            "avg_sigma": round(avg, 6),
            "p95_sigma": round(p95, 6),
            "verdict_distribution": dict(verdict_counts),
            "latency_p50_ms": round(statistics.median(lat_all), 3) if lat_all else 0.0,
            "n_requests": n,
            "total_cost_units": round(self._cost_units, 6),
        }

    def per_layer_trace(self, steps: List[Dict[str, Any]]) -> None:
        for s in steps:
            self._layer_sigmas.append(
                {
                    "layer": s.get("layer"),
                    "sigma": s.get("sigma"),
                    "info": s.get("info"),
                },
            )

    def cost_tracking(self, *, route: str, sigma: float, cheap: bool = True) -> None:
        base = 0.1 if cheap and float(sigma) < 0.5 else 1.0
        mult = 1.0 + float(sigma)
        self._cost_units += base * mult

    def record_latency_ms(self, ms: float) -> None:
        self._latencies_ms.append(float(ms))

    @property
    def layer_traces(self) -> List[Dict[str, Any]]:
        return list(self._layer_sigmas)
