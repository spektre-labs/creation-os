# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""LangChain / LiteLLM / decorator σ integrations (optional deps where noted)."""
from __future__ import annotations

import uuid
from datetime import datetime, timedelta
from types import SimpleNamespace

import pytest


def test_callback_tracks_sigma() -> None:
    pytest.importorskip("langchain_core")
    from cos.integrations.langchain_sigma import SigmaCallback

    cb = SigmaCallback(on_abstain="log")
    run_id = uuid.uuid4()
    cb.on_llm_start({}, ["hello"], run_id=run_id)
    gen = SimpleNamespace(text="world", generation_info=None)
    resp = SimpleNamespace(generations=[[gen]])
    cb.on_llm_end(resp, run_id=run_id)
    assert len(cb.traces) == 1
    assert "sigma" in cb.traces[0]
    assert isinstance(gen.generation_info, dict)
    assert "sigma" in gen.generation_info
    assert "σ" in gen.generation_info


def test_callback_summary() -> None:
    pytest.importorskip("langchain_core")
    from cos.integrations.langchain_sigma import SigmaCallback

    cb = SigmaCallback(on_abstain="log")
    for i in range(2):
        rid = uuid.uuid4()
        cb.on_llm_start({}, [f"prompt-{i}"], run_id=rid)
        g = SimpleNamespace(text="ok", generation_info=None)
        cb.on_llm_end(SimpleNamespace(generations=[[g]]), run_id=rid)
    s = cb.summary()
    assert s["count"] == 2
    assert "σ_avg" in s
    assert "accept_rate" in s


def test_decorator_wraps_function() -> None:
    from cos.integrations.decorator import SigmaResult, sigma_gated

    @sigma_gated
    def echo(p: str) -> str:
        return "out"

    r = echo("hello")
    assert isinstance(r, SigmaResult)
    assert r.text == "out"
    assert r.prompt == "hello"


def test_decorator_block_abstain() -> None:
    from cos.integrations.decorator import SigmaResult, sigma_gated

    class _Gate:
        def score(self, prompt: str, response: str) -> tuple[float, str]:
            del prompt, response
            return (0.99, "ABSTAIN")

    @sigma_gated(gate=_Gate(), block_abstain=True)
    def f(p: str) -> str:
        return "x"

    r = f("q")
    assert isinstance(r, SigmaResult)
    assert "BLOCKED" in r.text
    assert r.verdict == "ABSTAIN"


def test_litellm_callback_logs() -> None:
    from cos.integrations.litellm_sigma import SigmaLiteLLMCallback

    cb = SigmaLiteLLMCallback()
    t0 = datetime.now()
    t1 = t0 + timedelta(milliseconds=50)
    ro = SimpleNamespace(choices=[SimpleNamespace(message=SimpleNamespace(content="ok"))])
    cb.log_success_event(
        {"model": "unit-test", "messages": [{"role": "user", "content": "hi"}]},
        ro,
        t0,
        t1,
    )
    assert len(cb.trace) == 1
    assert cb.trace[0]["model"] == "unit-test"
    assert "sigma" in cb.trace[0]
    assert cb.trace[0]["latency_ms"] >= 0.0

    cb.log_failure_event({"model": "unit-test-fail"}, None, t0, t1)
    assert cb.trace[-1].get("error") is True
    assert cb.trace[-1]["verdict"] == "ABSTAIN"
