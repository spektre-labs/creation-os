# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-metric — in-process unified metrics (lab).

Prometheus text and OTLP-shaped export are **string/dict helpers**, not a full SDK.
Downstream may swap for OpenTelemetry / Prometheus client. See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import json
import math
import statistics
import time
from collections import defaultdict
from typing import Any, DefaultDict, Dict, List, Mapping, Optional, Sequence

__all__ = ["SigmaMetrics"]


class SigmaMetrics:
    """Counters, gauges, histograms, tagged samples; rollups for dashboards."""

    def __init__(self) -> None:
        self._samples: List[Dict[str, Any]] = []
        self._counters: DefaultDict[str, float] = defaultdict(float)
        self._gauges: Dict[str, float] = {}
        self._histograms: DefaultDict[str, List[float]] = defaultdict(list)
        self._verdict_counts: DefaultDict[str, int] = defaultdict(int)
        self._latencies: List[float] = []
        self._costs: List[float] = []
        self._started = time.monotonic()

    def record(self, name: str, value: float, tags: Optional[Mapping[str, str]] = None) -> None:
        self._samples.append(
            {
                "name": str(name),
                "value": float(value),
                "tags": dict(tags or {}),
                "ts": time.time(),
            }
        )

    def counter(self, name: str, delta: float = 1.0) -> float:
        self._counters[str(name)] += float(delta)
        return float(self._counters[str(name)])

    def histogram(self, name: str, value: float) -> None:
        self._histograms[str(name)].append(float(value))

    def gauge(self, name: str, value: float) -> None:
        self._gauges[str(name)] = float(value)

    def observe_verdict(self, verdict: str) -> None:
        self._verdict_counts[str(verdict).upper()] += 1

    def observe_latency_ms(self, ms: float) -> None:
        self._latencies.append(float(ms))

    def observe_cost_eur(self, eur: float) -> None:
        self._costs.append(float(eur))

    @staticmethod
    def _percentile(sorted_vals: Sequence[float], p: float) -> float:
        if not sorted_vals:
            return 0.0
        k = (len(sorted_vals) - 1) * (p / 100.0)
        f = math.floor(k)
        c = math.ceil(k)
        if f == c:
            return float(sorted_vals[int(k)])
        d0 = sorted_vals[f] * (c - k)
        d1 = sorted_vals[c] * (k - f)
        return float(d0 + d1)

    def standard_metrics(self) -> Dict[str, Any]:
        sigmas = [float(s["value"]) for s in self._samples if str(s["name"]).startswith("sigma")]
        sigmas.sort()
        lat_sorted = sorted(self._latencies)
        total_v = sum(self._verdict_counts.values()) or 1
        return {
            "sigma_avg": round(float(statistics.mean(sigmas)), 6) if sigmas else 0.0,
            "sigma_p50": round(self._percentile(sigmas, 50), 6) if sigmas else 0.0,
            "sigma_p95": round(self._percentile(sigmas, 95), 6) if sigmas else 0.0,
            "sigma_p99": round(self._percentile(sigmas, 99), 6) if sigmas else 0.0,
            "accept_rate": round(self._verdict_counts.get("ACCEPT", 0) / total_v, 6),
            "rethink_rate": round(self._verdict_counts.get("RETHINK", 0) / total_v, 6),
            "abstain_rate": round(self._verdict_counts.get("ABSTAIN", 0) / total_v, 6),
            "latency_ms_p50": round(self._percentile(lat_sorted, 50), 3),
            "latency_ms_p95": round(self._percentile(lat_sorted, 95), 3),
            "throughput_qps": round(
                len(self._latencies) / max(time.monotonic() - self._started, 1e-6),
                6,
            ),
            "cost_per_query_eur": round(
                float(statistics.mean(self._costs)) if self._costs else 0.0,
                8,
            ),
            "drift_score": round(float(self._gauges.get("drift_score", 0.0)), 6),
        }

    def export_prometheus(self) -> str:
        lines: List[str] = []
        for k, v in self._counters.items():
            lines.append(f"cos_counter{{{self._escape(k)}}} {v}")
        for k, v in self._gauges.items():
            lines.append(f"cos_gauge{{{self._escape(k)}}} {v}")
        for name, vals in self._histograms.items():
            if vals:
                lines.append(f"cos_histogram_sum{{{self._escape(name)}}} {sum(vals)}")
                lines.append(f"cos_histogram_count{{{self._escape(name)}}} {len(vals)}")
        std = self.standard_metrics()
        for key, val in std.items():
            if isinstance(val, (int, float)):
                lines.append(f'cos_standard{{metric="{key}"}} {val}')
        return "\n".join(lines) + "\n"

    @staticmethod
    def _escape(s: str) -> str:
        return f'name="{str(s).replace(chr(34), chr(92)+chr(34))}"'

    def export_json(self) -> str:
        return json.dumps(
            {
                "counters": dict(self._counters),
                "gauges": dict(self._gauges),
                "histograms": {k: vals for k, vals in self._histograms.items()},
                "standard": self.standard_metrics(),
                "samples_head": self._samples[:200],
            },
            indent=2,
            default=str,
        )

    def export_opentelemetry(self) -> Dict[str, Any]:
        """OTLP JSON resource/logs-style envelope (metrics as log records; lab)."""
        return {
            "resourceLogs": [
                {
                    "resource": {
                        "attributes": [
                            {"key": "service.name", "value": {"stringValue": "creation-os"}},
                            {"key": "cos.component", "value": {"stringValue": "sigma_metrics"}},
                        ]
                    },
                    "scopeLogs": [
                        {
                            "logRecords": [
                                {
                                    "timeUnixNano": str(int(time.time() * 1e9)),
                                    "severityText": "INFO",
                                    "body": {"stringValue": json.dumps(self.standard_metrics(), default=str)},
                                }
                            ]
                        }
                    ],
                }
            ]
        }

    def dashboard_summary(self) -> Dict[str, Any]:
        return {
            "standard_metrics": self.standard_metrics(),
            "counters": dict(self._counters),
            "gauges": dict(self._gauges),
            "histogram_names": list(self._histograms.keys()),
            "uptime_s": round(time.monotonic() - self._started, 3),
        }
