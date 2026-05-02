# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""v140 σ-agent: tool loop + structured output lab tests."""
from __future__ import annotations

import json
from typing import Any, Dict, List

from cos.sigma_agent import SigmaAgent, SigmaTool
from cos.sigma_gate_core import Verdict
from cos.sigma_structured import SigmaStructured


class _LowSigmaGate:
    def score(self, prompt: str, response: str):
        _ = prompt
        if "error" in response.lower():
            return 0.95, Verdict.ABSTAIN
        return 0.05, Verdict.ACCEPT


class _Search(SigmaTool):
    def __init__(self) -> None:
        super().__init__(
            "search",
            "toy",
            {"type": "object", "required": ["q"], "properties": {"q": {"type": "string"}}},
        )

    def run(self, **kwargs: Any) -> Dict[str, Any]:
        return {"q": kwargs.get("q"), "hit": "ok"}


class _ThreeToolsModel:
    def __init__(self) -> None:
        self._i = 0

    def generate(self, context: List[Dict[str, Any]], tools: List[Any]) -> Dict[str, Any]:
        _ = context, tools
        self._i += 1
        if self._i == 1:
            return {"tool_call": {"name": "search", "arguments": {"q": "a"}}}
        if self._i == 2:
            return {"tool_call": {"name": "search", "arguments": {"q": "b"}}}
        if self._i == 3:
            return {"tool_call": {"name": "search", "arguments": {"q": "c"}}}
        return {"content": "done (toy)."}


def test_sigma_agent_three_tool_calls_all_low_sigma_accept() -> None:
    ag = SigmaAgent(_ThreeToolsModel(), _LowSigmaGate(), [_Search()], max_steps=12)
    out = ag.run("compare three queries")
    assert out.get("verdict") == "ACCEPT"
    steps = [t for t in out.get("trace", []) if t.get("action") == "tool_call"]
    assert len(steps) == 3
    for s in steps:
        assert float(s.get("pre_sigma", 1.0)) < 0.2
        assert float(s.get("post_sigma", 1.0)) < 0.2
        assert s.get("verdict") == "ACCEPT"


class _HallucinatedToolModel:
    def __init__(self) -> None:
        self._done = False

    def generate(self, context: List[Dict[str, Any]], tools: List[Any]) -> Dict[str, Any]:
        _ = context, tools
        if not self._done:
            self._done = True
            return {"tool_call": {"name": "web_search", "arguments": {"q": "x"}}}
        return {"content": "fallback"}


def test_sigma_agent_unknown_tool_pre_tool_abstain() -> None:
    ag = SigmaAgent(_HallucinatedToolModel(), _LowSigmaGate(), [_Search()], max_steps=5)
    out = ag.run("search the web")
    blocked = [t for t in out.get("trace", []) if t.get("action") == "tool_blocked"]
    assert blocked, out
    assert "unknown" in str(blocked[0].get("reason", "")).lower()


class _BadArgsModel:
    def __init__(self) -> None:
        self._once = False

    def generate(self, context: List[Dict[str, Any]], tools: List[Any]) -> Dict[str, Any]:
        _ = context, tools
        if not self._once:
            self._once = True
            return {"tool_call": {"name": "search", "arguments": {}}}
        return {"content": "noop"}


def test_sigma_agent_invalid_args_abstain() -> None:
    ag = SigmaAgent(_BadArgsModel(), _LowSigmaGate(), [_Search()], max_steps=4)
    out = ag.run("query")
    blocked = [t for t in out.get("trace", []) if t.get("action") == "tool_blocked"]
    assert blocked
    assert "invalid" in str(blocked[0].get("reason", "")).lower()


class _StructModel:
    def __init__(self, wrong: bool) -> None:
        self._wrong = wrong

    def generate(self, prompt: str, *, response_format: Any = None) -> str:
        _ = prompt, response_format
        if self._wrong:
            return json.dumps({"city": "Helsinki", "population": 3})
        return json.dumps({"city": "Helsinki", "population": 658457})


class _PopFieldGate:
    """Flags nonsense population via per-field prompts (lab)."""

    def score(self, prompt: str, response: str):
        p = str(prompt).lower()
        if "population" in p and "3" in p:
            return 0.88, Verdict.ABSTAIN
        return 0.06, Verdict.ACCEPT


def test_sigma_structured_valid_shape_wrong_value_flagged() -> None:
    schema: Dict[str, Any] = {
        "type": "object",
        "required": ["city", "population"],
        "properties": {
            "city": {"type": "string"},
            "population": {"type": "integer"},
        },
    }
    ss = SigmaStructured(_PopFieldGate())
    out = ss.generate_and_validate(_StructModel(wrong=True), "Helsinki population?", schema)
    assert out.get("data", {}).get("population") == 3
    assert "population" in (out.get("high_sigma_fields") or [])
