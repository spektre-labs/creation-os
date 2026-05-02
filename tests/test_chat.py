# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.chat` (OpenAI-compatible σ-gated chat)."""
from __future__ import annotations

import json
from unittest.mock import MagicMock

import pytest

from cos import SigmaGate
from cos.sigma_gate import ABSTAIN, ACCEPT, RETHINK


def test_chat_single_turn_mock_openai() -> None:
    pytest.importorskip("openai")
    from cos import chat as chat_mod

    fake_resp = MagicMock()
    fake_resp.choices = [MagicMock(message=MagicMock(content="4"))]

    client = MagicMock()
    client.chat.completions.create.return_value = fake_resp

    gate = SigmaGate()
    messages: list[dict[str, str]] = [{"role": "user", "content": "What is 2+2?"}]
    _, sigma, verdict, assistant = chat_mod.run_turn(
        gate=gate,
        client=client,
        model="Qwen/Qwen3.6-35B-A3B",
        messages=messages,
        preserve_thinking=True,
    )
    assert assistant == "4"
    assert verdict in (ACCEPT, RETHINK, ABSTAIN)
    assert 0.0 <= sigma <= 1.0
    client.chat.completions.create.assert_called_once()
    assert len(messages) == 2
    assert messages[-1]["role"] == "assistant"


def test_chat_multi_turn_preserves_history() -> None:
    pytest.importorskip("openai")
    from cos import chat as chat_mod

    r1 = MagicMock()
    r1.choices = [MagicMock(message=MagicMock(content="first"))]
    r2 = MagicMock()
    r2.choices = [MagicMock(message=MagicMock(content="second"))]

    client = MagicMock()
    client.chat.completions.create.side_effect = [r1, r2]

    gate = SigmaGate()
    messages: list[dict[str, str]] = [{"role": "user", "content": "a"}]
    chat_mod.run_turn(
        gate=gate, client=client, model="m", messages=messages, preserve_thinking=False
    )
    messages.append({"role": "user", "content": "b"})
    chat_mod.run_turn(
        gate=gate, client=client, model="m", messages=messages, preserve_thinking=False
    )
    assert len(messages) == 4
    assert messages[1]["role"] == "assistant"
    assert messages[2]["role"] == "user"
    assert client.chat.completions.create.call_count == 2
    second_call_kw = client.chat.completions.create.call_args_list[1].kwargs
    assert len(second_call_kw["messages"]) == 3


def test_chat_abstain_shows_warning() -> None:
    from cos import chat as chat_mod

    line, code = chat_mod.format_user_output(verdict=ABSTAIN, sigma=0.95, text="guess", json_mode=False)
    assert "don't know" in line.lower()
    assert code == 2


def test_chat_rethink_shows_sigma() -> None:
    from cos import chat as chat_mod

    line, code = chat_mod.format_user_output(verdict=RETHINK, sigma=0.55, text="maybe", json_mode=False)
    assert "verify" in line
    assert "0.550" in line or "0.55" in line
    assert code == 0


def test_chat_no_endpoint_gives_error() -> None:
    pytest.importorskip("openai")
    from cos import chat as chat_mod

    client = MagicMock()
    client.chat.completions.create.side_effect = OSError("connection refused")

    gate = SigmaGate()
    out: list[str] = []

    def _capture(*a: object, **k: object) -> None:
        out.append(" ".join(str(x) for x in a))

    code = chat_mod.chat_single_turn(
        gate=gate,
        client=client,
        model="m",
        prompt="hi",
        preserve_thinking=False,
        json_mode=False,
        verbose=False,
        print_fn=_capture,
    )
    assert code == 1
    assert any("error" in x.lower() for x in out)


def test_chat_json_output_format() -> None:
    from cos import chat as chat_mod

    line, code = chat_mod.format_user_output(verdict=ACCEPT, sigma=0.1, text="ok", json_mode=True)
    d = json.loads(line)
    assert d["verdict"] == ACCEPT
    assert d["sigma"] == pytest.approx(0.1)
    assert d["text"] == "ok"
    assert code == 0


def test_chat_preserve_thinking_flag() -> None:
    from cos import chat as chat_mod

    k = chat_mod.build_completion_kwargs(
        model="Qwen/Qwen3.6-35B-A3B",
        messages=[],
        preserve_thinking=True,
    )
    assert k.get("extra_body", {}).get("chat_template_kwargs", {}).get("preserve_thinking") is True

    k2 = chat_mod.build_completion_kwargs(model="gpt-4o", messages=[], preserve_thinking=True)
    assert "extra_body" not in k2

    k3 = chat_mod.build_completion_kwargs(
        model="Qwen/Qwen3.6-35B-A3B",
        messages=[],
        preserve_thinking=False,
    )
    assert "extra_body" not in k3


def test_assistant_text_from_completion_dict() -> None:
    from cos import chat as chat_mod

    raw = {"choices": [{"message": {"content": "hello"}}]}
    assert chat_mod.assistant_text_from_completion_dict(raw) == "hello"
