# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.epistemic` (σ-band policy + exploration plan lab)."""
from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.epistemic import EpistemicAgent  # noqa: E402


class _FixedSigmaGate:
    def __init__(self, sigma: float) -> None:
        self._sigma = float(sigma)

    def score(self, _prompt: str, _response: str):
        return self._sigma, "ACCEPT"


class _ExploreThenLearnGate:
    """High σ until an ``after ...`` follow-up prompt, then low σ."""

    def __init__(self, before: float = 0.82, after: float = 0.08) -> None:
        self._before = float(before)
        self._after = float(after)

    def score(self, prompt: str, _response: str):
        if "after " in str(prompt):
            return self._after, "ACCEPT"
        return self._before, "ABSTAIN"


def test_assess_known_topic() -> None:
    agent = EpistemicAgent(gate=_FixedSigmaGate(0.1))
    a = agent.assess("algebra", "I know linear equations well.")
    assert a["mode"] == "ASSERT"
    assert a["σ"] == 0.1
    assert agent.knowledge_map["algebra"]["mode"] == "ASSERT"


def test_assess_unknown_topic() -> None:
    agent = EpistemicAgent(gate=_FixedSigmaGate(0.72))
    a = agent.assess("dark matter", "")
    assert a["mode"] == "EXPLORE"
    assert a["σ"] == 0.72


def test_explore_plan_generates_steps() -> None:
    agent = EpistemicAgent(gate=_FixedSigmaGate(0.65))
    agent.assess("quaternions", "uncertain")
    plan = agent.explore_plan("quaternions")
    assert "steps" in plan
    assert len(plan["steps"]) == 3
    assert all("σ_feasibility" in s for s in plan["steps"])
    assert agent.explorations


def test_execute_exploration_reduces_sigma() -> None:
    gate = _ExploreThenLearnGate()
    agent = EpistemicAgent(gate=gate)
    agent.assess("tensor networks", "unsure")
    assert agent.knowledge_map["tensor networks"]["mode"] == "EXPLORE"
    rep = agent.execute_exploration(
        "tensor networks",
        "Found a survey paper with definitions and diagrams.",
        "search",
    )
    assert rep["σ_before"] > rep["σ_after"]
    assert rep["improvement"] > 0
    assert rep["learned"] is True
    assert agent.knowledge_map["tensor networks"]["action"] == "learned"


def test_knowledge_frontier_maps() -> None:
    class _TriGate:
        def score(self, prompt: str, _response: str):
            t = str(prompt)
            if "topic_a" in t:
                return 0.1, "ACCEPT"
            if "topic_b" in t:
                return 0.35, "RETHINK"
            return 0.7, "ABSTAIN"

    agent = EpistemicAgent(gate=_TriGate())
    agent.assess("topic_a", "x")
    agent.assess("topic_b", "y")
    agent.assess("topic_c", "z")
    f = agent.knowledge_frontier()
    assert len(f["known"]) == 1
    assert len(f["partially_known"]) == 1
    assert len(f["unknown"]) == 1
    assert f["total_topics"] == 3


def test_curiosity_signal_highest_sigma() -> None:
    class _DualExplore:
        def score(self, prompt: str, _response: str):
            if "topic_low" in str(prompt):
                return 0.55, "ABSTAIN"
            return 0.9, "ABSTAIN"

    agent = EpistemicAgent(gate=_DualExplore())
    agent.assess("topic_low", "")
    agent.assess("topic_high", "")
    sig = agent.curiosity_signal()
    assert sig["most_curious"] == "topic_high"
    assert sig["σ"] == 0.9


def test_estimate_steps_to_knowledge() -> None:
    agent = EpistemicAgent(gate=_FixedSigmaGate(0.5))
    assert agent._estimate_steps(0.15, 0.2, 0.15) == 0
    assert agent._estimate_steps(0.8, 0.2, 0.15) == 5
