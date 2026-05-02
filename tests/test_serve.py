# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""HTTP tests for :mod:`cos.serve` (FastAPI). Requires ``creation-os[serve]``."""
from __future__ import annotations

from unittest.mock import patch

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


def test_score_with_persona_query(client: TestClient) -> None:
    """Optional ``persona`` applies persona τ overrides for the request, then restores defaults."""
    r = client.post(
        "/v1/score?persona=medical",
        json={"prompt": "What is 2+2?", "response": "4"},
    )
    assert r.status_code == 200
    assert "sigma" in r.json()


def test_plugins_list(client: TestClient) -> None:
    r = client.get("/v1/plugins")
    assert r.status_code == 200
    body = r.json()
    assert "plugins" in body
    assert isinstance(body["plugins"], list)


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


def test_report_endpoint(client: TestClient) -> None:
    r = client.post(
        "/v1/report",
        json={
            "kind": "mtier",
            "format": "markdown",
            "bench_results": {"demo": {"auroc": "—"}},
        },
    )
    assert r.status_code == 200
    assert "CLAIM_DISCIPLINE" in r.json()["body"]


def test_evidence(client: TestClient) -> None:
    r = client.get("/v1/evidence")
    assert r.status_code == 200
    body = r.json()
    assert body.get("status") == "NOT_AGI_ACHIEVED"


def test_metrics_prometheus(client: TestClient) -> None:
    r = client.get("/metrics")
    assert r.status_code == 200
    assert "cos_" in r.text


class _FakeAsyncClient:
    """Minimal async HTTP client for patching ``httpx.AsyncClient``."""

    def __init__(self, *a: object, **k: object) -> None:
        pass

    async def __aenter__(self) -> _FakeAsyncClient:
        return self

    async def __aexit__(self, *a: object) -> None:
        return None

    async def post(self, url: str, json: object = None, headers: object = None) -> object:
        class _Resp:
            status_code = 200
            headers = {"content-type": "application/json"}

            def json(self) -> dict[str, object]:
                return {"choices": [{"message": {"content": "assistant hello"}}]}

        return _Resp()


def test_v1_chat_completions_proxies(client: TestClient) -> None:
    with patch("cos.serve.httpx.AsyncClient", _FakeAsyncClient):
        r = client.post(
            "/v1/chat/completions",
            json={"model": "m", "messages": [{"role": "user", "content": "hi"}], "stream": False},
        )
    assert r.status_code == 200
    data = r.json()
    assert data["choices"][0]["message"]["content"] == "assistant hello"
    assert "creation_os" in data
    assert data["creation_os"]["verdict"] in ("ACCEPT", "RETHINK", "ABSTAIN")


def test_v1_chat_completions_stream_rejected(client: TestClient) -> None:
    r = client.post(
        "/v1/chat/completions",
        json={"model": "m", "messages": [], "stream": True},
    )
    assert r.status_code == 400
