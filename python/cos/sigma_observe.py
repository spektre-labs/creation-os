# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
σ-observe — production-oriented traces where each record carries gate σ and verdict.

σ is the internal signal competitors typically approximate with external evaluators or LLM-judges.
Does not modify ``sigma_gate.h``.
"""
from __future__ import annotations

import json
import secrets
import time
import urllib.request
from collections import deque
from typing import Any, Deque, Dict, List, Optional, Protocol, Tuple, runtime_checkable

from .sigma_gate_core import Verdict


@runtime_checkable
class SigmaGateScoreFn(Protocol):
    def score(self, prompt: str, text: str) -> Tuple[float, Verdict]:
        ...


def _verdict_counts_empty() -> Dict[str, int]:
    return {v.name: 0 for v in Verdict}


class SigmaMetrics:
    """Rolling aggregates for σ, gate latency, and verdict mix."""

    def __init__(self) -> None:
        self.sigmas: Deque[float] = deque(maxlen=100_000)
        self.latencies: Deque[float] = deque(maxlen=100_000)
        self.verdicts: Dict[str, int] = _verdict_counts_empty()

    def update(self, entry: Dict[str, Any]) -> None:
        self.sigmas.append(float(entry["sigma"]))
        self.latencies.append(float(entry["gate_latency_ms"]))
        v = str(entry["verdict"])
        if v not in self.verdicts:
            self.verdicts[v] = 0
        self.verdicts[v] += 1

    def avg_sigma(self) -> float:
        return float(sum(self.sigmas) / max(len(self.sigmas), 1))

    def percentile(self, p: float) -> float:
        if not self.sigmas:
            return 0.0
        s = sorted(self.sigmas)
        if len(s) == 1:
            return float(s[0])
        p = float(min(100.0, max(0.0, p)))
        k = (len(s) - 1) * (p / 100.0)
        f = int(k)
        c = min(f + 1, len(s) - 1)
        if f == c:
            return float(s[f])
        return float(s[f] * (c - k) + s[c] * (k - f))

    def trend(self, window: int = 100) -> str:
        """Trend over the last 2×window samples: rising σ ⇒ degrading."""
        w = int(window)
        if len(self.sigmas) < w * 2:
            return "insufficient_data"
        seq = list(self.sigmas)
        old = seq[-w * 2 : -w]
        new = seq[-w:]
        old_avg = sum(old) / max(len(old), 1)
        new_avg = sum(new) / max(len(new), 1)
        delta = new_avg - old_avg
        if delta > 0.05:
            return "degrading"
        if delta < -0.05:
            return "improving"
        return "stable"

    def verdict_counts(self) -> Dict[str, int]:
        return dict(self.verdicts)

    def avg_latency(self) -> float:
        return float(sum(self.latencies) / max(len(self.latencies), 1))


class SigmaAlertEngine:
    """σ-driven alerts (consecutive high-σ spike, abstain visibility)."""

    def __init__(self) -> None:
        self.active_alerts: List[Dict[str, Any]] = []
        self.consecutive_high = 0

    def check(self, entry: Dict[str, Any]) -> None:
        sigma = float(entry["sigma"])
        if sigma > 0.8:
            self.consecutive_high += 1
            if self.consecutive_high >= 3:
                self._fire(
                    "sigma_spike_3x",
                    "critical",
                    f"3 consecutive σ > 0.8 (last={sigma:.3f})",
                )
        else:
            self.consecutive_high = 0

        if str(entry["verdict"]) == Verdict.ABSTAIN.name:
            self._fire("abstain", "info", f"ABSTAIN: σ={sigma:.3f}")

    def _fire(self, name: str, severity: str, message: str) -> None:
        self.active_alerts.append(
            {
                "name": name,
                "severity": severity,
                "message": message,
                "timestamp": time.time(),
            }
        )

    def active_count(self) -> int:
        return len(self.active_alerts)


def _random_trace_id() -> str:
    return secrets.token_hex(16)


def _random_span_id() -> str:
    return secrets.token_hex(8)


def export_otel_span(trace_entry: Dict[str, Any]) -> Dict[str, Any]:
    """OTel-style span dictionary (JSON-serializable; wire via OTLP HTTP in ``post_otlp_http``)."""
    tid = str(trace_entry.get("trace_id") or _random_trace_id())
    return {
        "traceId": tid,
        "spanId": str(trace_entry.get("span_id") or _random_span_id()),
        "name": "sigma_gate",
        "kind": "INTERNAL",
        "attributes": {
            "sigma.value": float(trace_entry["sigma"]),
            "sigma.verdict": str(trace_entry["verdict"]),
            "sigma.gate_latency_ms": float(trace_entry["gate_latency_ms"]),
            "sigma.model": str(trace_entry.get("model", "")),
        },
    }


def build_otlp_http_body(trace_entry: Dict[str, Any]) -> Dict[str, Any]:
    """Minimal OTLP JSON trace payload (single span; lab / Jaeger-compatible HTTP collectors)."""
    tid = trace_entry.get("trace_id") or _random_trace_id()
    sid = trace_entry.get("span_id") or _random_span_id()
    start = int(float(trace_entry.get("timestamp", time.time())) * 1_000_000_000)
    lat_ns = int(float(trace_entry.get("gate_latency_ms", 0.0)) * 1_000_000)
    end = start + max(lat_ns, 1000)
    span = {
        "traceId": tid if len(tid) == 32 else secrets.token_hex(16),
        "spanId": sid if len(sid) == 16 else secrets.token_hex(8),
        "name": "sigma_gate",
        "kind": 1,
        "startTimeUnixNano": str(start),
        "endTimeUnixNano": str(end),
        "attributes": [
            {"key": "sigma.value", "value": {"doubleValue": float(trace_entry["sigma"])}},
            {"key": "sigma.verdict", "value": {"stringValue": str(trace_entry["verdict"])}},
            {"key": "sigma.gate_latency_ms", "value": {"doubleValue": float(trace_entry["gate_latency_ms"])}},
            {"key": "sigma.model", "value": {"stringValue": str(trace_entry.get("model", ""))}},
        ],
    }
    return {
        "resourceSpans": [
            {
                "resource": {
                    "attributes": [
                        {"key": "service.name", "value": {"stringValue": "creation-os-sigma-observe"}},
                    ],
                },
                "scopeSpans": [{"scope": {"name": "cos.sigma_observe"}, "spans": [span]}],
            },
        ],
    }


def post_otlp_http(endpoint: str, trace_entry: Dict[str, Any], timeout_s: float = 2.0) -> None:
    """POST a single span to an OTLP/HTTP traces endpoint (best-effort; raises on hard errors)."""
    url = endpoint.rstrip("/")
    if not url.endswith("/v1/traces"):
        url = f"{url}/v1/traces"
    body = json.dumps(build_otlp_http_body(trace_entry)).encode("utf-8")
    req = urllib.request.Request(
        url,
        data=body,
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    with urllib.request.urlopen(req, timeout=timeout_s) as resp:
        if resp.status >= 300:
            raise RuntimeError(f"OTLP export HTTP {resp.status}")


class SigmaObserve:
    """Ring buffer of traces + metrics + alerts; optional OTLP export."""

    def __init__(
        self,
        gate: Optional[SigmaGateScoreFn] = None,
        *,
        buffer_size: int = 10_000,
        otel_endpoint: Optional[str] = None,
    ) -> None:
        self.gate: Optional[SigmaGateScoreFn] = gate
        self.traces: Deque[Dict[str, Any]] = deque(maxlen=int(buffer_size))
        self.metrics = SigmaMetrics()
        self.alerts = SigmaAlertEngine()
        self._otel_endpoint = (otel_endpoint or "").strip() or None
        self._otel_export_failures = 0

    def set_otel_endpoint(self, url: Optional[str]) -> None:
        self._otel_endpoint = (url or "").strip() or None

    def _maybe_otel(self, entry: Dict[str, Any]) -> None:
        if not self._otel_endpoint:
            return
        try:
            post_otlp_http(self._otel_endpoint, entry)
        except Exception:
            self._otel_export_failures += 1

    def trace_llm(
        self,
        prompt: str,
        response: str,
        model: str,
        metadata: Optional[Dict[str, Any]] = None,
    ) -> Dict[str, Any]:
        """Score (prompt, response) with the configured gate and append a trace record."""
        if self.gate is None:
            raise ValueError("SigmaObserve.trace_llm requires a gate (construct with gate=...)")
        t0 = time.monotonic()
        sigma, verdict = self.gate.score(str(prompt), str(response))
        gate_latency = (time.monotonic() - t0) * 1000.0
        trace_id = _random_trace_id()
        span_id = _random_span_id()
        entry = {
            "trace_id": trace_id,
            "span_id": span_id,
            "timestamp": time.time(),
            "prompt": str(prompt)[:200],
            "response": str(response)[:200],
            "model": str(model),
            "sigma": float(sigma),
            "verdict": verdict.name,
            "gate_latency_ms": float(gate_latency),
            "metadata": dict(metadata or {}),
        }
        self.traces.append(entry)
        self.metrics.update(entry)
        self.alerts.check(entry)
        self._maybe_otel(entry)
        return entry

    def ingest_rows(self, rows: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
        """
        Legacy batch ingest (``cos observe --from-file``): rows already carry σ / verdict.
        """
        out: List[Dict[str, Any]] = []
        for row in rows:
            if not isinstance(row, dict):
                continue
            vr = row.get("verdict", Verdict.ACCEPT.name)
            if isinstance(vr, Verdict):
                verdict_s = vr.name
            else:
                verdict_s = str(vr)
            sigma = float(row.get("sigma", 0.0))
            entry = {
                "trace_id": _random_trace_id(),
                "span_id": _random_span_id(),
                "timestamp": time.time(),
                "prompt": str(row.get("prompt", ""))[:200],
                "response": str(row.get("response", ""))[:200],
                "model": str(row.get("model", "ingest")),
                "sigma": sigma,
                "verdict": verdict_s,
                "gate_latency_ms": 0.0,
                "metadata": {
                    "source": "ingest",
                    "step": row.get("step"),
                    "signals": row.get("signals", {}),
                },
            }
            self.traces.append(entry)
            self.metrics.update(entry)
            self.alerts.check(entry)
            self._maybe_otel(entry)
            out.append({**row, "recorded": True})
        return out

    def trace(self, first: Any, second: Optional[str] = None, third: Optional[str] = None) -> Any:
        """
        - ``trace(rows: list)`` → legacy ingest from JSON / JSONL.
        - ``trace(prompt, response, model?)`` → live gate scoring (needs ``gate``).
        """
        if isinstance(first, list):
            return self.ingest_rows(first)
        if second is None:
            raise TypeError("trace(prompt, response, model?, ...) requires response when prompt is not a list")
        model = str(third or "unknown")
        return self.trace_llm(str(first), str(second), model, metadata=None)

    def dashboard(self) -> Dict[str, Any]:
        return {
            "total_traces": len(self.traces),
            "avg_sigma": self.metrics.avg_sigma(),
            "verdict_distribution": self.metrics.verdict_counts(),
            "sigma_p50": self.metrics.percentile(50),
            "sigma_p95": self.metrics.percentile(95),
            "sigma_p99": self.metrics.percentile(99),
            "avg_gate_latency_ms": self.metrics.avg_latency(),
            "sigma_trend": self.metrics.trend(window=100),
            "alerts_active": self.alerts.active_count(),
        }

    def export_otel(self, trace_entry: Dict[str, Any]) -> Dict[str, Any]:
        return export_otel_span(trace_entry)


__all__ = [
    "SigmaAlertEngine",
    "SigmaGateScoreFn",
    "SigmaMetrics",
    "SigmaObserve",
    "build_otlp_http_body",
    "export_otel_span",
    "post_otlp_http",
]
