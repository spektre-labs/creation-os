# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for ``cos.sigma_swarm``."""
from __future__ import annotations

import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.sigma_gate_core import SigmaState, sigma_q16  # noqa: E402
from cos.sigma_swarm import SigmaSwarm  # noqa: E402


def _accept_state(sigma_f: float = 0.4) -> SigmaState:
    return SigmaState(sigma=sigma_q16(sigma_f), d_sigma=0, k_eff=sigma_q16(0.5))


def _abstain_state() -> SigmaState:
    return SigmaState(sigma=sigma_q16(0.5), d_sigma=sigma_q16(-0.1), k_eff=sigma_q16(0.05))


def _rethink_state() -> SigmaState:
    return SigmaState(sigma=sigma_q16(0.4), d_sigma=sigma_q16(0.05), k_eff=sigma_q16(0.5))


@dataclass
class MutableAgent:
    id: str
    text: str
    state: SigmaState = field(default_factory=_accept_state)

    def execute(self, task: Any) -> str:
        return self.text

    def get_sigma_state(self) -> SigmaState:
        return self.state


@dataclass
class RetryAgent:
    id: str
    state: SigmaState

    def execute(self, task: Any) -> str:
        return "first"

    def get_sigma_state(self) -> SigmaState:
        return self.state

    def retry_with_ttt(self, task: Any) -> str:
        self.state = _accept_state(0.35)
        return "second"


def test_swarm_consensus_prefers_lower_sigma() -> None:
    a = MutableAgent("low", "A", _accept_state(0.1))
    b = MutableAgent("high", "B", _accept_state(0.9))
    out = SigmaSwarm([a, b]).orchestrate("task")
    assert out["verdict"] == "CONSENSUS"
    assert out["winner_agent_id"] == "low"
    assert out["output"] == "A"


def test_swarm_single_accept() -> None:
    a = MutableAgent("only", "ok", _accept_state())
    out = SigmaSwarm([a]).orchestrate("t")
    assert out["verdict"] == "SINGLE_ACCEPT"
    assert out["payload"]["output"] == "ok"


def test_swarm_abstain_all() -> None:
    a = MutableAgent("x", "bad", _abstain_state())
    b = MutableAgent("y", "bad2", _abstain_state())
    out = SigmaSwarm([a, b]).orchestrate("t")
    assert out["verdict"] == "SWARM_ABSTAIN"


def test_rethink_retry_accept() -> None:
    ag = RetryAgent("r", _rethink_state())
    out = SigmaSwarm([ag]).orchestrate("t")
    assert out["verdict"] == "SINGLE_ACCEPT"
    assert out["payload"]["verdict"] == "ACCEPT_AFTER_RETRY"
    assert out["payload"]["output"] == "second"


def test_rethink_retry_still_fails_omits() -> None:
    @dataclass
    class BadRetry:
        id: str
        state: SigmaState = field(default_factory=_rethink_state)

        def execute(self, task: Any) -> str:
            return "a"

        def get_sigma_state(self) -> SigmaState:
            return self.state

        def retry_with_ttt(self, task: Any) -> str:
            self.state = _rethink_state()
            return "b"

    out = SigmaSwarm([BadRetry("z")]).orchestrate("t")
    assert out["verdict"] == "SWARM_ABSTAIN"
