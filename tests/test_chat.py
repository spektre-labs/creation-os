# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.chat` (``SigmaChat`` + OpenAI-compatible backends)."""
from __future__ import annotations

import json
from unittest.mock import MagicMock, patch

import pytest

from cos.sigma_gate import ABSTAIN, SigmaGate


def test_chat_single_turn_returns_sigma() -> None:
    pytest.importorskip("openai")
    from cos.chat import SigmaChat

    fake_resp = MagicMock()
    fake_resp.choices = [MagicMock(message=MagicMock(content="four"))]

    with patch.object(SigmaChat, "__init__", lambda s, **k: None):
        c = SigmaChat.__new__(SigmaChat)
        c.endpoint = "http://localhost:8000/v1"
        c.model = "Qwen/Qwen3.6-35B-A3B"
        c.preserve_thinking = True
        c.gate = SigmaGate()
        c.messages = []
        c.history = []
        client = MagicMock()
        client.chat.completions.create.return_value = fake_resp
        c.client = client

        r = SigmaChat.send(c, "What is 2+2?")
    assert r["error"] is None
    assert r["text"] == "four"
    assert r["verdict"] in ("ACCEPT", "RETHINK", "ABSTAIN")
    assert 0.0 <= float(r["sigma"]) <= 1.0
    client.chat.completions.create.assert_called_once()


def test_chat_multi_turn_preserves_messages() -> None:
    pytest.importorskip("openai")
    from cos.chat import SigmaChat

    r1 = MagicMock()
    r1.choices = [MagicMock(message=MagicMock(content="A"))]
    r2 = MagicMock()
    r2.choices = [MagicMock(message=MagicMock(content="B"))]

    with patch.object(SigmaChat, "__init__", lambda s, **k: None):
        c = SigmaChat.__new__(SigmaChat)
        c.endpoint = "http://localhost:8000/v1"
        c.model = "m"
        c.preserve_thinking = False
        c.gate = SigmaGate()
        c.messages = []
        c.history = []
        client = MagicMock()
        client.chat.completions.create.side_effect = [r1, r2]
        c.client = client

        SigmaChat.send(c, "a")
        SigmaChat.send(c, "b")
    assert len(c.messages) == 4
    assert client.chat.completions.create.call_count == 2
    kw2 = client.chat.completions.create.call_args_list[1].kwargs
    assert len(kw2["messages"]) == 3


def test_chat_abstain_on_error() -> None:
    pytest.importorskip("openai")
    from cos.chat import SigmaChat

    with patch.object(SigmaChat, "__init__", lambda s, **k: None):
        c = SigmaChat.__new__(SigmaChat)
        c.endpoint = "http://localhost:8000/v1"
        c.model = "m"
        c.preserve_thinking = False
        c.gate = SigmaGate()
        c.messages = []
        c.history = []
        client = MagicMock()
        client.chat.completions.create.side_effect = RuntimeError("upstream down")
        c.client = client

        r = SigmaChat.send(c, "hi")
    assert r["text"] is None
    assert r["verdict"] == ABSTAIN
    assert r["error"] == "upstream down"


def test_chat_reset_clears_history() -> None:
    pytest.importorskip("openai")
    from cos.chat import SigmaChat

    with patch.object(SigmaChat, "__init__", lambda s, **k: None):
        c = SigmaChat.__new__(SigmaChat)
        c.messages = [{"role": "user", "content": "x"}]
        c.history = [("x", "y", 0.1, "ACCEPT")]
        SigmaChat.reset(c)
    assert c.messages == []
    assert c.history == []


def test_chat_session_sigma_average() -> None:
    pytest.importorskip("openai")
    from cos.chat import SigmaChat

    with patch.object(SigmaChat, "__init__", lambda s, **k: None):
        c = SigmaChat.__new__(SigmaChat)
        c.history = [("a", "b", 0.2, "ACCEPT"), ("c", "d", 0.4, "RETHINK")]
    assert SigmaChat.session_sigma(c) == pytest.approx(0.3)


def test_chat_json_output() -> None:
    from cos.chat import format_user_output

    line, code = format_user_output(verdict="ACCEPT", sigma=0.12, text="ok", json_mode=True)
    d = json.loads(line)
    assert d["verdict"] == "ACCEPT"
    assert d["text"] == "ok"
    assert code == 0


def test_chat_no_openai_raises_import_error() -> None:
    import cos.chat as chat_mod

    prev = chat_mod._HAS_OPENAI
    chat_mod._HAS_OPENAI = False
    try:
        with pytest.raises(ImportError, match="openai"):
            chat_mod.SigmaChat()
    finally:
        chat_mod._HAS_OPENAI = prev


def test_send_stream_sets_preserve_thinking_extra_body_for_qwen() -> None:
    pytest.importorskip("openai")
    from cos.chat import SigmaChat

    captured: dict = {}

    class _Delta:
        def __init__(self, content: str) -> None:
            self.content = content

    class _Ch:
        def __init__(self, content: str) -> None:
            self.delta = _Delta(content)

    class _Chunk:
        def __init__(self, content: str) -> None:
            self.choices = [_Ch(content)]

    def _fake_create(**kwargs: object) -> list:
        captured.update(kwargs)
        return [_Chunk("a"), _Chunk("b")]

    with patch.object(SigmaChat, "__init__", lambda s, **k: None):
        c = SigmaChat.__new__(SigmaChat)
        c.endpoint = "http://localhost:8000/v1"
        c.model = "Qwen/Qwen3.6-35B-A3B"
        c.preserve_thinking = True
        c.gate = SigmaGate()
        c.messages = []
        c.history = []
        client = MagicMock()
        client.chat.completions.create = _fake_create
        c.client = client
        list(SigmaChat.send_stream(c, "ping"))

    eb = captured.get("extra_body") or {}
    assert eb.get("chat_template_kwargs", {}).get("preserve_thinking") is True
    assert captured.get("stream") is True


def test_parse_thinking_tags() -> None:
    """Parse ``<think>...</think>`` from assistant ``content``."""
    pytest.importorskip("openai")
    from cos.chat import SigmaChat

    raw = (
        "<think>Let me reason about this</think>"
        "The answer is 42"
    )
    fake_resp = MagicMock()
    fake_resp.model = "Qwen/Qwen3.6-35B-A3B"
    fake_resp.choices = [
        MagicMock(message=MagicMock(content=raw, reasoning_content=None))
    ]

    chat = SigmaChat.__new__(SigmaChat)
    chat.model = "Qwen/Qwen3.6-35B-A3B"
    parsed = SigmaChat._parse_response(chat, fake_resp)
    assert parsed["thinking"] == "Let me reason about this"
    assert parsed["content"] == "The answer is 42"


def test_sigma_scores_content_not_thinking() -> None:
    """σ-gate receives stripped assistant content only (not chain-of-thought)."""
    pytest.importorskip("openai")
    from cos.chat import SigmaChat

    raw = (
        "<think>secret chain</think>"
        "The answer is 42"
    )
    fake_resp = MagicMock()
    fake_resp.model = "Qwen/Qwen3.6-35B-A3B"
    fake_resp.choices = [
        MagicMock(message=MagicMock(content=raw, reasoning_content=None))
    ]

    gate = MagicMock()
    gate.score.return_value = (0.1, "ACCEPT")

    with patch.object(SigmaChat, "__init__", lambda s, **k: None):
        c = SigmaChat.__new__(SigmaChat)
        c.endpoint = "http://localhost:8000/v1"
        c.model = "Qwen/Qwen3.6-35B-A3B"
        c.preserve_thinking = True
        c.gate = gate
        c.messages = []
        c.history = []
        client = MagicMock()
        client.chat.completions.create.return_value = fake_resp
        c.client = client

        SigmaChat.send(c, "What is the answer?")

    gate.score.assert_called_once()
    assert gate.score.call_args[0][0] == "What is the answer?"
    assert gate.score.call_args[0][1] == "The answer is 42"
    assert "secret" not in gate.score.call_args[0][1]


def test_build_extra_body_dashscope_top_level_preserve() -> None:
    from cos.chat import build_extra_body

    eb = build_extra_body("https://dashscope.aliyuncs.com/compatible-mode/v1", True, "Qwen/Qwen3.6-35B-A3B")
    assert eb == {"preserve_thinking": True}
