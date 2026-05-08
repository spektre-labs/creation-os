# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""End-to-end request trace — spans, σ per stage, JSON + OTLP-shaped export (lab).

Complements :class:`cos.observe.SigmaObserve` as a **request-scoped** tree. Not a full
OpenTelemetry SDK; exports dicts you can POST to a collector if you add transport.
"""
from __future__ import annotations

from cos.sigma_gate import SigmaGate  # noqa: F401 — σ kernel anchor (sigma_gate.h / Python lite)

import json
import time
import uuid
from contextlib import contextmanager
from typing import Any, Dict, Iterator, List, Optional

__all__ = ["SigmaTrace"]


class SigmaTrace:
    """One trace_id; append spans with timing and optional σ / verdict."""

    SPAN_NAMES = (
        "input_guard",
        "tokenize",
        "retrieve",
        "generate",
        "sigma_gate",
        "output_guard",
    )

    def __init__(self, trace_id: Optional[str] = None) -> None:
        self.trace_id = trace_id or str(uuid.uuid4())
        self.spans: List[Dict[str, Any]] = []

    @contextmanager
    def span(
        self,
        name: str,
        *,
        metadata: Optional[Dict[str, Any]] = None,
    ) -> Iterator[Dict[str, Any]]:
        """Record duration; set ``sigma`` / ``verdict`` on the record dict inside the block."""
        t0 = time.perf_counter()
        rec: Dict[str, Any] = {
            "name": name,
            "start_ms": round(t0 * 1000.0, 3),
            "end_ms": None,
            "duration_ms": None,
            "sigma": None,
            "verdict": None,
            "metadata": dict(metadata or {}),
        }
        self.spans.append(rec)
        try:
            yield rec
        finally:
            t1 = time.perf_counter()
            rec["end_ms"] = round(t1 * 1000.0, 3)
            rec["duration_ms"] = round((t1 - t0) * 1000.0, 3)

    def export_json(self) -> str:
        return json.dumps(
            {"trace_id": self.trace_id, "spans": self.spans},
            ensure_ascii=False,
            indent=2,
        )

    def export_opentelemetry(self) -> Dict[str, Any]:
        """Minimal OTLP JSON resource-spans shape (no binary protobuf)."""
        return {
            "resourceSpans": [
                {
                    "resource": {
                        "attributes": [{"key": "service.name", "value": {"stringValue": "creation-os"}}]
                    },
                    "scopeSpans": [
                        {
                            "spans": [
                                {
                                    "traceId": self.trace_id.replace("-", ""),
                                    "spanId": format(i, "016x"),
                                    "name": s["name"],
                                    "startTimeUnixNano": int(float(s["start_ms"]) * 1_000_000),
                                    "endTimeUnixNano": int(float(s.get("end_ms") or s["start_ms"]) * 1_000_000),
                                    "attributes": [
                                        {"key": k, "value": {"stringValue": str(v)}}
                                        for k, v in {
                                            "cos.sigma": s.get("sigma"),
                                            "cos.verdict": s.get("verdict"),
                                            **{f"cos.meta.{k}": v for k, v in (s.get("metadata") or {}).items()},
                                        }.items()
                                        if v is not None
                                    ],
                                }
                                for i, s in enumerate(self.spans)
                            ]
                        }
                    ],
                }
            ]
        }

    def waterfall_view(self) -> str:
        """Human-readable timeline (ASCII)."""
        lines = [f"trace_id={self.trace_id}", "-" * 40]
        for s in self.spans:
            sig = s.get("sigma")
            v = s.get("verdict")
            meta = f" σ={sig} {v}" if sig is not None else ""
            dur = s.get("duration_ms")
            lines.append(f"  {s['name']:<16} {dur} ms{meta}")
        return "\n".join(lines)

    def bottleneck_detection(self) -> Dict[str, Any]:
        """Slowest span and highest σ span (if σ recorded)."""
        slow = None
        high = None
        for s in self.spans:
            d = s.get("duration_ms")
            if d is not None and (slow is None or d > slow[1]):
                slow = (s["name"], float(d))
            sig = s.get("sigma")
            if sig is not None and (high is None or float(sig) > high[1]):
                high = (s["name"], float(sig))
        return {
            "slowest": {"name": slow[0], "duration_ms": slow[1]} if slow else None,
            "highest_sigma": {"name": high[0], "sigma": high[1]} if high else None,
        }
