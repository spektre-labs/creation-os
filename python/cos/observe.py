# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-observe — in-process tracing + histograms (OpenTelemetry-shaped dicts, no hard OTel dep).

Suitable for feeding ``/metrics`` or a dashboard in a full stack; this file stays
dependency-light. See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import statistics
import time
from collections import Counter
from typing import Any, Dict, List, Optional

__all__ = ["SigmaObserve"]


class SigmaObserve:
    """Record spans, σ samples, drift checks, and coarse cost attribution."""

    def __init__(self, *, max_samples: int = 10_000) -> None:
        self.max_samples = max(100, int(max_samples))
        self._spans: List[Dict[str, Any]] = []
        self._sigma_hist: List[float] = []
        self._latencies_ms: List[float] = []
        self._verdict_counts: Counter[str] = Counter()
        self._layer_sigmas: List[Dict[str, Any]] = []
        self._cost_units: float = 0.0

    def trace_request(
        self,
        prompt: str,
        response: Optional[str],
        sigma: float,
        verdict: str,
        *,
        trace_name: str = "cos.request",
    ) -> Dict[str, Any]:
        """Append an OTEL-like span record (lab JSON, not exported to a collector here)."""
        span = {
            "name": trace_name,
            "start_ns": time.time_ns(),
            "attributes": {
                "cos.prompt_len": len(str(prompt)),
                "cos.response_len": len(str(response or "")),
                "cos.sigma": round(float(sigma), 6),
                "cos.verdict": str(verdict),
            },
        }
        self._spans.append(span)
        self._sigma_hist.append(float(sigma))
        self._verdict_counts[str(verdict)] += 1
        self._trim()
        return span

    def sigma_histogram(self) -> Dict[str, Any]:
        """Return coarse bin counts for drift visualization."""
        if not self._sigma_hist:
            return {"bins": [], "counts": [], "n": 0}
        bins = [0.0, 0.25, 0.5, 0.75, 1.0]
        counts = [0, 0, 0, 0]
        for s in self._sigma_hist:
            for i in range(len(bins) - 1):
                if bins[i] <= s < bins[i + 1] or (i == len(bins) - 2 and s == 1.0):
                    counts[i] += 1
                    break
        return {"bins": bins, "counts": counts, "n": len(self._sigma_hist)}

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
        sigs = list(self._sigma_hist)
        n = len(sigs)
        avg = sum(sigs) / max(n, 1)
        if n == 0:
            p95 = 0.0
        else:
            srt = sorted(sigs)
            p95 = float(srt[int((n - 1) * 0.95)])
        lat = list(self._latencies_ms)
        return {
            "avg_sigma": round(avg, 6),
            "p95_sigma": round(p95, 6),
            "verdict_distribution": dict(self._verdict_counts),
            "latency_p50_ms": round(statistics.median(lat), 3) if lat else 0.0,
            "n_requests": n,
            "total_cost_units": round(self._cost_units, 6),
        }

    def per_layer_trace(self, steps: List[Dict[str, Any]]) -> None:
        """Ingest fabric ``SigmaTrace.steps``-like rows for bottleneck skew."""
        for s in steps:
            self._layer_sigmas.append(
                {
                    "layer": s.get("layer"),
                    "sigma": s.get("sigma"),
                    "info": s.get("info"),
                },
            )

    def cost_tracking(self, *, route: str, sigma: float, cheap: bool = True) -> None:
        """σ-routed billing stub: expensive path when σ is high or ``cheap`` is False."""
        base = 0.1 if cheap and float(sigma) < 0.5 else 1.0
        mult = 1.0 + float(sigma)
        self._cost_units += base * mult

    def record_latency_ms(self, ms: float) -> None:
        self._latencies_ms.append(float(ms))
        self._trim()

    def _trim(self) -> None:
        if len(self._sigma_hist) > self.max_samples:
            self._sigma_hist = self._sigma_hist[-self.max_samples :]
        if len(self._spans) > self.max_samples:
            self._spans = self._spans[-self.max_samples :]

    @property
    def layer_traces(self) -> List[Dict[str, Any]]:
        return list(self._layer_sigmas)
