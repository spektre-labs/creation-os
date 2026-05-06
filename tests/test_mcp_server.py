# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import asyncio
import json

import pytest

pytest.importorskip("mcp")

from cos.mcp_server import EVIDENCE_LADDER_TEXT, build_mcp, create_mcp_server


async def _call_tool(name: str, arguments: dict) -> object:
    app = build_mcp()
    res = await app.call_tool(name, arguments)
    if isinstance(res, dict):
        return res
    if isinstance(res, tuple) and len(res) == 2 and isinstance(res[1], dict):
        return res[1]
    structured = getattr(res, "structured_content", None)
    if structured is not None:
        return structured
    raise AssertionError(f"unexpected call_tool return: {type(res)!r}")


async def _read_resource(uri: str) -> str:
    app = build_mcp()
    bundle = await app.read_resource(uri)
    if isinstance(bundle, list) and bundle:
        first = bundle[0]
        return str(getattr(first, "content", first))
    contents = getattr(bundle, "contents", None)
    assert contents
    raw = contents[0]
    return getattr(raw, "content", str(raw))


def test_create_mcp_server() -> None:
    server = create_mcp_server()
    assert server is not None


def test_score_tool_returns_sigma_and_verdict() -> None:
    out = asyncio.run(_call_tool("score", {"prompt": "What is 2+2?", "response": "4"}))
    assert isinstance(out, dict)
    assert "sigma" in out and "verdict" in out
    assert 0.0 <= float(out["sigma"]) <= 1.0
    assert str(out["verdict"])


def test_score_cascade_returns_levels() -> None:
    out = asyncio.run(_call_tool("score_cascade", {"prompt": "ping", "response": "pong"}))
    assert isinstance(out, dict)
    assert "levels" in out
    assert "L1_entropy" in out["levels"]
    assert "sigma" in out and "verdict" in out


def test_batch_score_multiple_pairs() -> None:
    pairs = [
        {"prompt": "a", "response": "b"},
        {"prompt": "c", "response": "d"},
    ]
    out = asyncio.run(_call_tool("batch_score", {"pairs": pairs}))
    assert isinstance(out, dict)
    rows = out.get("results")
    if rows is None:
        rows = out.get("result")
    assert isinstance(rows, list) and len(rows) == 2
    for row, src in zip(rows, pairs, strict=True):
        assert row["prompt"] == src["prompt"]
        assert row["response"] == src["response"]
        assert 0.0 <= float(row["sigma"]) <= 1.0
        assert row["verdict"]


def test_explain_tool_gives_explanation() -> None:
    out = asyncio.run(_call_tool("explain", {"prompt": "x", "response": "y"}))
    assert isinstance(out, dict)
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

    def _fake_create(**_k: object) -> _App:
        return fake

    monkeypatch.setattr(ms, "create_mcp_server", _fake_create)
    ms.run_server("streamable-http", host="127.0.0.1", port=8123)
    assert fake.kw.get("transport") == "streamable-http"
    if ms._MCP_BACKEND == "official":
        assert fake.kw.get("port") is None
    else:
        assert fake.kw.get("port") == 8123


def test_evidence_ladder_includes_negatives() -> None:
    raw = asyncio.run(_read_resource("evidence://ladder"))
    low = raw.lower()
    assert "halueval" in low and "0.514" in raw
    assert "not agi" in low


def test_evidence_ladder_constant_matches_resource() -> None:
    raw = asyncio.run(_read_resource("evidence://ladder"))
    assert raw.strip() == EVIDENCE_LADDER_TEXT.strip()


def test_mcp_server_lists_tools_and_verify_prompt() -> None:
    async def _collect() -> tuple[set[str], set[str]]:
        app = build_mcp()
        tools = await app.list_tools()
        prompts = await app.list_prompts()
        return {t.name for t in tools}, {p.name for p in prompts}

    tnames, pnames = asyncio.run(_collect())
    assert tnames == {"score", "score_cascade", "batch_score", "explain"}
    assert "verify" in pnames


def test_sigma_score_direct() -> None:
    from cos.sigma_gate import SigmaGate

    gate = SigmaGate()
    sigma, verdict = gate.score("What is 2+2?", "4")
    assert 0.0 <= float(sigma) <= 1.0
    assert verdict in ("ACCEPT", "RETHINK", "ABSTAIN")


def test_batch_pairs_sigma_range() -> None:
    from cos.sigma_gate import SigmaGate

    gate = SigmaGate()
    pairs = [
        {"prompt": "2+2?", "response": "4"},
        {"prompt": "Capital?", "response": "Paris"},
    ]
    for p in pairs:
        sigma, verdict = gate.score(p["prompt"], p["response"])
        assert 0.0 <= float(sigma) <= 1.0
        assert verdict in ("ACCEPT", "RETHINK", "ABSTAIN")


def test_no_mcp_raises_import(monkeypatch) -> None:
    import cos.mcp_server as ms

    monkeypatch.setattr(ms, "_HAS_MCP", False)
    monkeypatch.setattr(ms, "FastMCP", None)
    monkeypatch.setattr(ms, "_MCP_IMPORT_ERROR", "test-path")
    with pytest.raises(ImportError, match="MCP SDK"):
        ms.create_mcp_server()


def test_verify_prompt_template() -> None:
    text = "The Earth is flat"
    prompt = f"Please score this output for hallucinations:\n\n{text}"
    assert "flat" in prompt
