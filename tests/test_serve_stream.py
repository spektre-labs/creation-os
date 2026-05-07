# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""SSE ``/v1/stream`` tests (σ per prefix + end-of-stream summary)."""
from __future__ import annotations

import json

import pytest

pytest.importorskip("fastapi")

from fastapi.testclient import TestClient  # noqa: E402

from cos.serve import create_app  # noqa: E402


def _parse_sse_events(body: str) -> list[dict]:
    events: list[dict] = []
    for line in body.splitlines():
        s = line.strip()
        if s.startswith("data:"):
            raw = s[5:].strip()
            if raw:
                events.append(json.loads(raw))
    return events


@pytest.fixture
def client() -> TestClient:
    return TestClient(create_app())


def test_stream_returns_sse_events(client: TestClient) -> None:
    r = client.post(
        "/v1/stream",
        json={"prompt": "test", "tokens": ["hello", "world"]},
    )
    assert r.status_code == 200
    evs = _parse_sse_events(r.text)
    assert len(evs) >= 3


def test_stream_final_event_has_done(client: TestClient) -> None:
    r = client.post("/v1/stream", json={"prompt": "p", "tokens": ["a", "b"]})
    assert r.status_code == 200
    evs = _parse_sse_events(r.text)
    assert evs[-1].get("done") is True
    assert "σ_final" in evs[-1]
    assert "verdict_final" in evs[-1]
    assert evs[-1].get("total_tokens") == 2


def test_stream_sigma_per_token(client: TestClient) -> None:
    r = client.post("/v1/stream", json={"prompt": "What is 2+2?", "tokens": ["4"]})
    assert r.status_code == 200
    evs = _parse_sse_events(r.text)
    tok_ev = evs[0]
    assert tok_ev.get("done") is False
    assert "σ" in tok_ev
    assert 0.0 <= float(tok_ev["σ"]) <= 1.0
    assert "accumulated_length" in tok_ev


def test_stream_content_type(client: TestClient) -> None:
    r = client.post("/v1/stream", json={"prompt": "x", "response": "one two"})
    assert r.status_code == 200
    assert r.headers.get("content-type", "").startswith("text/event-stream")
    assert r.headers.get("x-accel-buffering") == "no"
    assert "no-cache" in (r.headers.get("cache-control") or "")
