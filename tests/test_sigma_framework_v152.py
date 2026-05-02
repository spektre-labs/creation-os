# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
"""v152 framework integration smoke tests (optional third-party deps)."""
from __future__ import annotations

import os
import sys
from types import SimpleNamespace
from uuid import uuid4

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "python"))


def test_langchain_callback_traces_per_llm_end() -> None:
    pytest.importorskip("langchain_core")

    from cos.integrations.langchain_sigma import SigmaGateCallback

    class _Gen:
        def __init__(self, text: str) -> None:
            self.text = text
            self.generation_info = None

    class _Resp:
        def __init__(self, text: str) -> None:
            self.generations = [[_Gen(text)]]

    cb = SigmaGateCallback(on_abstain="log")
    rid = uuid4()
    cb.on_llm_start({}, ["What is the capital of France?"], run_id=rid)
    cb.on_llm_end(_Resp("Berlin"), run_id=rid)
    assert len(cb.traces) == 1
    assert cb.traces[0]["verdict"] == "ABSTAIN"
    assert cb.traces[0]["sigma"] >= 0.5

    rid2 = uuid4()
    cb.on_llm_start({}, ["noop"], run_id=rid2)
    cb.on_llm_end(_Resp("Paris"), run_id=rid2)
    assert len(cb.traces) == 2


def test_crewai_tool_run_accept() -> None:
    pytest.importorskip("crewai")
    from cos.integrations.crewai_sigma import SigmaGateTool

    tool = SigmaGateTool()
    out = tool._run("What is 2+2?", "4")
    assert "verdict=ACCEPT" in out.replace(" ", "")


def test_autogen_hook_abstain_suffix() -> None:
    from cos.integrations.autogen_sigma import SigmaAutoGenHook

    class _Gate:
        def score(self, prompt: str, response: str) -> tuple[float, str]:
            _ = prompt
            _ = response
            return (0.99, "ABSTAIN")

    hook = SigmaAutoGenHook(_Gate())
    msgs: list[dict[str, str]] = [
        {"role": "user", "content": "Q?"},
        {"role": "assistant", "content": "guess"},
    ]
    hook.process_last_received_message(msgs, sender=None)
    assert msgs[-1]["verdict"] == "ABSTAIN"
    assert "σ-gate WARNING" in msgs[-1]["content"]


def test_integrations_cli_check_runs() -> None:
    from cos.cli import _cmd_integrations

    rc = _cmd_integrations(SimpleNamespace(integrations_check=True))
    assert rc == 0


def test_integrations_cli_example_requires_framework_flag() -> None:
    from cos.cli import _cmd_integrations

    rc = _cmd_integrations(SimpleNamespace(integrations_example=True))
    assert rc == 2
