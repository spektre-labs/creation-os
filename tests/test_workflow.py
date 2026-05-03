# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.workflow`."""
from __future__ import annotations

from cos.sigma_gate import SigmaGate
from cos.workflow import SigmaWorkflow


def _good(st: dict) -> dict:
    st["prompt"] = "What is 2+2?"
    st["response"] = "4"
    return st


def test_simple_3_step_workflow() -> None:
    wf = SigmaWorkflow()
    spec = wf.define(
        [
            {"name": "a", "fn": _good, "sigma_threshold": 1.0, "max_retries": 2},
            {"name": "b", "fn": _good, "sigma_threshold": 1.0, "max_retries": 2},
            {"name": "c", "fn": _good, "sigma_threshold": 1.0, "max_retries": 2},
        ]
    )
    out = wf.execute(spec, "run")
    assert out["ok"] is True
    assert len(out["step_sigmas"]) == 3


def test_rethink_triggers_retry() -> None:
    gate = SigmaGate()

    def flip(st: dict) -> dict:
        n = int(st.get("_n", 0))
        st["_n"] = n + 1
        if n == 0:
            st["prompt"], st["response"] = "ping", "pong"
        else:
            st["prompt"], st["response"] = "What is 2+2?", "4"
        return st

    wf = SigmaWorkflow(gate=gate)
    spec = wf.define(
        [{"name": "one", "fn": flip, "sigma_threshold": 1.0, "max_retries": 3}],
    )
    out = wf.execute(spec, "x")
    assert out["ok"] is True
    assert int(out["state"].get("_n", 0)) >= 2


def test_abstain_stops_workflow() -> None:

    def bad(st: dict) -> dict:
        st["prompt"] = "q"
        st["response"] = ""
        return st

    wf = SigmaWorkflow()
    spec = wf.define(
        [
            {"name": "ok", "fn": _good, "sigma_threshold": 1.0, "max_retries": 1},
            {"name": "bad", "fn": bad, "sigma_threshold": 1.0, "max_retries": 1},
        ]
    )
    out = wf.execute(spec, "x")
    assert out["ok"] is False
    assert out["stopped"] and out["stopped"]["reason"] == "abstain"


def test_checkpoint_survives_crash() -> None:
    wf = SigmaWorkflow()
    spec = wf.define(
        [{"name": "s1", "fn": _good, "checkpoint": True, "sigma_threshold": 1.0, "max_retries": 1}]
    )
    st0 = {"input": "run", "prompt": "init", "response": "", "cookie": 42}
    st1 = dict(spec["steps"][0]["fn"](dict(st0)))
    wf.checkpoint(spec["steps"][0], st1)
    assert wf._checkpoints[-1]["state"]["cookie"] == 42


def test_rollback_to_previous_step() -> None:
    wf = SigmaWorkflow()
    s1 = {"name": "s1", "fn": lambda s: {**s, "v": 1}, "checkpoint": True, "sigma_threshold": 1.0, "max_retries": 1}
    spec = wf.define([s1])
    st = {"input": "x", "prompt": "What is 2+2?", "response": "4", "v": 0}
    st = dict(spec["steps"][0]["fn"](st))
    wf.checkpoint(spec["steps"][0], st)
    st2 = {**st, "v": 99}
    rb = wf.rollback({"name": "s1"})
    assert rb["ok"] and rb["state"]["v"] == 1
    assert st2["v"] == 99


def test_human_gate_blocks_execution() -> None:
    gate = SigmaGate()

    def pong_only(st: dict) -> dict:
        st["prompt"], st["response"] = "ping", "pong"
        return st

    deny = lambda step, sigma, verdict: False  # noqa: E731
    wf = SigmaWorkflow(gate=gate, human_gate_fn=deny)
    spec = wf.define(
        [
            {
                "name": "r1",
                "fn": pong_only,
                "sigma_threshold": 1.0,
                "max_retries": 0,
                "human_gate": True,
            }
        ]
    )
    out = wf.execute(spec, "x")
    assert out["ok"] is False
    assert out["stopped"]["reason"] == "human_gate_denied"


def test_parallel_selects_lowest_sigma() -> None:
    wf = SigmaWorkflow()
    base = {"input": "i", "prompt": "What is 2+2?", "response": ""}
    br = [
        {"name": "x", "fn": lambda s: {**s, "response": "x" * 90}},
        {"name": "y", "fn": lambda s: {**s, "response": "4"}},
    ]
    st2, sig, _v = wf.parallel(br, dict(base))
    assert st2["response"] == "4"
    assert sig < 0.4


def test_cascade_error_prevented_at_step_3() -> None:
    wf = SigmaWorkflow()
    spec = wf.define(
        [
            {"name": "t1", "fn": _good, "sigma_threshold": 1.0, "max_retries": 1},
            {"name": "t2", "fn": _good, "sigma_threshold": 1.0, "max_retries": 1},
            {"name": "t3", "fn": _good, "sigma_threshold": 1.0, "max_retries": 1},
            {"name": "t4", "fn": _good, "sigma_threshold": 1.0, "max_retries": 1},
        ]
    )
    out = wf.execute(spec, "run", cascade_limit=0.22)
    assert out["stopped"]["reason"] == "cascade_error_prevention"
    assert len(out["step_sigmas"]) == 3
