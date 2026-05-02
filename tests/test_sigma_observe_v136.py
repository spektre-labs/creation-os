# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
"""v136 σ-observe: metrics, dashboard JSON API, OTLP-shaped export, alerts."""
from __future__ import annotations

import json
import threading
import time
import urllib.request
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]


class _SpikeGate:
    """Emit σ > 0.8 exactly three times, then low σ (ACCEPT throughout)."""

    def __init__(self) -> None:
        self.n = 0

    def score(self, prompt: str, text: str):
        from cos.sigma_gate_core import Verdict

        self.n += 1
        if self.n <= 3:
            return 0.85, Verdict.ACCEPT
        return 0.1, Verdict.ACCEPT


def test_thousand_traces_dashboard_trend_stable() -> None:
    from cos.sigma_observe import SigmaObserve
    from cos.sigma_split import SigmaSplitGate

    gate = SigmaSplitGate()
    obs = SigmaObserve(gate=gate, buffer_size=20_000)
    for i in range(1000):
        obs.trace_llm(f"prompt-{i % 5}", "ok steady", "lab")
    dash = obs.dashboard()
    assert dash["total_traces"] == 1000
    assert dash["sigma_trend"] == "stable"


def test_three_consecutive_high_sigma_fires_alert() -> None:
    from cos.sigma_observe import SigmaObserve

    obs = SigmaObserve(gate=_SpikeGate())
    for _ in range(3):
        obs.trace_llm("x", "y", "m")
    crit = [a for a in obs.alerts.active_alerts if a.get("severity") == "critical"]
    assert len(crit) >= 1
    assert "sigma_spike_3x" in crit[-1].get("name", "")


def test_otlp_body_shape() -> None:
    from cos.sigma_observe import build_otlp_http_body

    entry = {
        "trace_id": "a" * 32,
        "span_id": "b" * 16,
        "timestamp": 1_700_000_000.0,
        "sigma": 0.2,
        "verdict": "ACCEPT",
        "gate_latency_ms": 1.5,
        "model": "toy",
    }
    body = build_otlp_http_body(entry)
    assert "resourceSpans" in body
    assert body["resourceSpans"][0]["scopeSpans"][0]["spans"][0]["name"] == "sigma_gate"


def test_http_dashboard_get_roundtrip() -> None:
    from cos.sigma_dashboard import serve_dashboard
    from cos.sigma_observe import SigmaObserve
    from cos.sigma_split import SigmaSplitGate

    obs = SigmaObserve(gate=SigmaSplitGate())
    obs.trace_llm("a", "brief", "m")
    port = 38474
    th = threading.Thread(
        target=lambda: serve_dashboard(obs, host="127.0.0.1", port=port),
        daemon=True,
    )
    th.start()
    time.sleep(0.2)
    with urllib.request.urlopen(f"http://127.0.0.1:{port}/v1/dashboard", timeout=2) as r:
        data = json.loads(r.read().decode("utf-8"))
    assert data["total_traces"] == 1
    assert "avg_sigma" in data


def test_cos_observe_mock_drift_cli() -> None:
    import os
    import subprocess
    import sys

    env = {**os.environ, "PYTHONPATH": str(REPO / "python")}
    proc = subprocess.run(
        [sys.executable, "-m", "cos", "observe", "--mock-drift", "--traces", "--alerts"],
        cwd=REPO,
        env=env,
        check=True,
        capture_output=True,
        text=True,
    )
    out = json.loads(proc.stdout)
    assert "traces" in out and len(out["traces"]) == 3
    assert "alerts" in out
