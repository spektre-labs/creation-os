# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Integration smoke tests (no live LLM APIs)."""
from __future__ import annotations

import time
import uuid
from types import SimpleNamespace

import pytest

from cos.integrations.decorator import SigmaAbstainError, SigmaResult, sigma_gated
from cos.integrations.openai_wrapper import sigma_chat


def test_decorator_returns_sigma_result() -> None:
    @sigma_gated
    def fake_llm(prompt: str) -> str:
        del prompt
        return "The answer is 42"

    result = fake_llm("What is the answer?")
    assert isinstance(result, SigmaResult)
    assert 0 <= result.sigma <= 1
    assert result.verdict in ("ACCEPT", "RETHINK", "ABSTAIN")
    assert "42" in result.text


def test_decorator_dict_payload() -> None:
    @sigma_gated(return_dict=True)
    def my_fn(prompt: str) -> str:
        del prompt
        return "The answer is 42"

    result = my_fn("What is the answer?")
    assert isinstance(result, dict)
    assert "sigma" in result
    assert "verdict" in result
    assert result["result"] == "The answer is 42"


@pytest.mark.slow
def test_decorator_dict_elapsed() -> None:
    @sigma_gated(return_dict=True)
    def slow_fn(prompt: str) -> str:
        del prompt
        time.sleep(0.01)
        return "done"

    result = slow_fn("test")
    assert float(result["elapsed_ms"]) > 0


@pytest.mark.integration
def test_langchain_sigma_callback_init() -> None:
    pytest.importorskip("langchain_core")
    from cos.integrations.langchain_sigma import SigmaCallback

    cb = SigmaCallback()
    assert cb.gate is not None
    assert cb.traces == []


@pytest.mark.integration
def test_langchain_sigma_callback_tracks() -> None:
    pytest.importorskip("langchain_core")
    from cos.integrations.langchain_sigma import SigmaCallback

    cb = SigmaCallback()
    run_id = uuid.uuid4()
    cb.on_llm_start({}, ["test prompt"], run_id=run_id)

    class FakeResponse:
        class Gen:
            text = "test response"

        generations = [[Gen()]]

    cb.on_llm_end(FakeResponse(), run_id=run_id)
    assert len(cb.traces) == 1
    assert "sigma" in cb.traces[0]
    assert "verdict" in cb.traces[0]


def test_sigma_chat_fake_client() -> None:
    class _Completions:
        @staticmethod
        def create(*args: object, **kwargs: object) -> SimpleNamespace:
            del args
            return SimpleNamespace(
                choices=[SimpleNamespace(message=SimpleNamespace(content="Assistant says hi"))]
            )

    client = SimpleNamespace(chat=SimpleNamespace(completions=_Completions()))
    out = sigma_chat(client, [{"role": "user", "content": "Hello"}])
    assert hasattr(out, "sigma")
    assert hasattr(out, "verdict")
    assert 0 <= float(out.sigma) <= 1
    assert str(out.verdict) in ("ACCEPT", "RETHINK", "ABSTAIN")


@pytest.mark.integration
def test_langchain_package_callback_when_installed() -> None:
    try:
        from langchain_core.outputs import Generation, LLMResult
    except ImportError:
        pytest.skip("langchain-core not installed")

    from cos.integrations.langchain import SigmaGateCallback

    cb = SigmaGateCallback()
    cb._last_prompts = ["What is 2+2?"]
    gen = Generation(text="4")
    gen.generation_info = None
    resp = LLMResult(generations=[[gen]])
    cb.on_llm_end(resp)
    assert cb.last_sigma is not None
    assert cb.last_verdict is not None
    assert cb.stats["total"] >= 1
    assert isinstance(gen.generation_info, dict)
    assert "sigma" in gen.generation_info


@pytest.mark.integration
def test_litellm_logger_when_installed() -> None:
    try:
        from cos.integrations.litellm import SigmaGateLiteLLM
    except ImportError:
        pytest.skip("litellm not installed")

    try:
        logger = SigmaGateLiteLLM()
    except ImportError:
        pytest.skip("litellm not installed")

    ro = SimpleNamespace(choices=[SimpleNamespace(message=SimpleNamespace(content="ok"))])
    logger.log_success_event(
        {"messages": [{"role": "user", "content": "ping"}]},
        ro,
        0.0,
        1.0,
    )
    assert hasattr(ro, "sigma")
    assert logger.stats["total"] >= 1


def test_decorator_raise_on_abstain() -> None:
    @sigma_gated(raise_on_abstain=True)
    def fake_llm(prompt: str) -> str:
        del prompt
        return ""

    with pytest.raises(SigmaAbstainError):
        fake_llm("test")
