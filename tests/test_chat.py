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
