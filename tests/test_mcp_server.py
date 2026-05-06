# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import asyncio
import json

import pytest

pytest.importorskip("fastmcp")

from cos.mcp_server import build_mcp


async def _call_tool(name: str, arguments: dict) -> dict:
    app = build_mcp()
    res = await app.call_tool(name, arguments)
    assert res.structured_content is not None
    return res.structured_content  # type: ignore[return-value]


async def _read_resource(uri: str) -> str:
    app = build_mcp()
    bundle = await app.read_resource(uri)
    assert bundle.contents
    raw = bundle.contents[0]
    return getattr(raw, "content", str(raw))


def test_score_tool_returns_sigma_and_verdict() -> None:
    out = asyncio.run(_call_tool("score", {"prompt": "What is 2+2?", "response": "4"}))
    assert "sigma" in out and "verdict" in out
    assert 0.0 <= float(out["sigma"]) <= 1.0
    assert str(out["verdict"])


def test_score_cascade_returns_levels() -> None:
    out = asyncio.run(_call_tool("score_cascade", {"prompt": "ping", "response": "pong"}))
    assert "levels" in out
    assert "L1_entropy" in out["levels"]
    assert "sigma" in out and "verdict" in out


def test_batch_score_multiple_pairs() -> None:
    pairs = [
        {"prompt": "a", "response": "b"},
        {"prompt": "c", "response": "d"},
    ]
    out = asyncio.run(_call_tool("batch_score", {"pairs": pairs}))
    rows = out["results"]
    assert isinstance(rows, list) and len(rows) == 2
    for row, src in zip(rows, pairs, strict=True):
        assert row["prompt"] == src["prompt"]
        assert row["response"] == src["response"]
        assert 0.0 <= float(row["sigma"]) <= 1.0
        assert row["verdict"]


def test_explain_tool_gives_explanation() -> None:
    out = asyncio.run(_call_tool("explain", {"prompt": "x", "response": "y"}))
    assert "explanation" in out
    assert "sigma" in out and "verdict" in out
    assert isinstance(out["explanation"], str)
    assert len(out["explanation"]) > 10


def test_thresholds_resource_returns_config() -> None:
    raw = asyncio.run(_read_resource("config://thresholds"))
    cfg = json.loads(raw)
    assert "threshold_accept" in cfg and "threshold_abstain" in cfg
    assert "default_threshold_accept" in cfg and "default_threshold_abstain" in cfg
    assert cfg.get("version") == "1.0.0"


def test_run_server_uses_streamable_http(monkeypatch) -> None:
    from cos import mcp_server as ms

    class _App:
        def __init__(self) -> None:
            self.kw: dict = {}

        def run(self, **kw: object) -> None:
            self.kw = dict(kw)

    fake = _App()
    monkeypatch.setattr(ms, "build_mcp", lambda: fake)
    ms.run_server("streamable-http", port=8123)
    assert fake.kw.get("transport") == "streamable-http"
    assert fake.kw.get("port") == 8123


def test_evidence_ladder_includes_negatives() -> None:
    raw = asyncio.run(_read_resource("evidence://ladder"))
    low = raw.lower()
    assert "halueval" in low and "0.514" in raw
    assert "not agi" in low


def test_mcp_v3_exposes_four_tools_only() -> None:
    async def _names() -> set[str]:
        app = build_mcp()
        tools = await app.list_tools()
        return {t.name for t in tools}

    assert asyncio.run(_names()) == {"score", "score_cascade", "batch_score", "explain"}
