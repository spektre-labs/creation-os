# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""HTTP tests for :mod:`cos.serve` (FastAPI). Requires ``creation-os[serve]``."""
from __future__ import annotations

import pytest

pytest.importorskip("fastapi")

from fastapi.testclient import TestClient  # noqa: E402

from cos.serve import create_app  # noqa: E402


@pytest.fixture
def client() -> TestClient:
    return TestClient(create_app())


def test_health(client: TestClient) -> None:
    r = client.get("/health")
    assert r.status_code == 200
    data = r.json()
    assert data["status"] == "ok"
    assert "version" in data
    assert data["sigma_gate"] == "operational"


def test_root(client: TestClient) -> None:
    r = client.get("/")
    assert r.status_code == 200
    j = r.json()
    assert j["service"] == "creation-os"
    assert j["docs"] == "/docs"


def test_score(client: TestClient) -> None:
    r = client.post(
        "/v1/score",
        json={"prompt": "What is 2+2?", "response": "4"},
    )
    assert r.status_code == 200
    data = r.json()
    assert "sigma" in data
    assert "verdict" in data
    assert 0 <= data["sigma"] <= 1


def test_pipe(client: TestClient) -> None:
    r = client.post("/v1/pipe", json={"prompt": "test", "response": "hello"})
    assert r.status_code == 200
    data = r.json()
    assert "sigma" in data
    assert "verdict" in data


def test_batch(client: TestClient) -> None:
    r = client.post(
        "/v1/batch",
        json={
            "items": [
                {"prompt": "a", "response": "b"},
                {"prompt": "c", "response": "d"},
            ]
        },
    )
    assert r.status_code == 200
    data = r.json()
    assert len(data["results"]) == 2


def test_stream_sse(client: TestClient) -> None:
    r = client.post("/v1/stream", json={"prompt": "test", "tokens": ["hello", "world"]})
    assert r.status_code == 200
    ct = r.headers.get("content-type", "")
    assert ct.startswith("text/event-stream")


def test_websocket_score(client: TestClient) -> None:
    with client.websocket_connect("/v1/ws") as ws:
        ws.send_json({"action": "score", "prompt": "test", "response": "hello"})
        data = ws.receive_json()
        assert "sigma" in data
        assert "verdict" in data


def test_websocket_ping(client: TestClient) -> None:
    with client.websocket_connect("/v1/ws") as ws:
        ws.send_json({"action": "ping"})
        data = ws.receive_json()
        assert data.get("pong") is True


def test_stats(client: TestClient) -> None:
    client.post("/v1/score", json={"prompt": "test", "response": "hello"})
    r = client.get("/v1/stats")
    assert r.status_code == 200
    assert r.json()["requests_total"] >= 1
    assert "pipeline" in r.json()


def test_evidence(client: TestClient) -> None:
    r = client.get("/v1/evidence")
    assert r.status_code == 200
    body = r.json()
    assert body.get("status") == "NOT_AGI_ACHIEVED"
