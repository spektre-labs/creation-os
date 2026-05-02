# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from cos.swarm import SigmaSwarm, SwarmMessage


def test_add_agent() -> None:
    swarm = SigmaSwarm()
    swarm.add_agent("researcher", "finds info")
    swarm.add_agent("writer", "writes")
    assert len(swarm.agents) == 2


def test_send_message() -> None:
    swarm = SigmaSwarm()
    swarm.add_agent("a", "sender")
    swarm.add_agent("b", "receiver")
    msg = swarm.send("a", "b", "The answer is 42", task="test")
    assert isinstance(msg, SwarmMessage)
    assert msg.sigma is not None
    assert msg.verdict is not None


def test_send_to_unknown_agent() -> None:
    swarm = SigmaSwarm()
    swarm.add_agent("a", "sender")
    msg = swarm.send("a", "nobody", "hello")
    assert msg.blocked is True


def test_execute_pipeline() -> None:
    swarm = SigmaSwarm()
    swarm.add_agent("step1", "first")
    swarm.add_agent("step2", "second")
    result = swarm.execute("test task", pipeline=["step1", "step2"])
    assert "trace" in result
    assert len(result["trace"]) == 2
    assert result["agents_used"] == 2


def test_trust_updates() -> None:
    swarm = SigmaSwarm()
    swarm.add_agent("a", "worker")
    swarm.add_agent("b", "receiver")
    swarm.send("a", "b", "good response", task="test")
    agent_a = swarm.agents["a"]
    assert len(agent_a.sigma_history) == 1
    assert 0 <= agent_a.trust_score <= 1


def test_trust_report() -> None:
    swarm = SigmaSwarm()
    swarm.add_agent("a", "worker")
    swarm.add_agent("b", "checker")
    swarm.send("a", "b", "hello", task="test")
    report = swarm.trust_report()
    assert "a" in report
    assert "trust" in report["a"]


def test_cascade_risk() -> None:
    swarm = SigmaSwarm()
    swarm.add_agent("good", "reliable")
    swarm.add_agent("bad", "unreliable")
    swarm.agents["bad"].trust_score = 0.2
    risk = swarm.cascade_risk()
    assert risk["bottleneck"] == "bad"
    assert risk["risk"] > 0.5


def test_block_high_sigma() -> None:
    swarm = SigmaSwarm(block_threshold=0.1)
    swarm.add_agent("a", "sender")
    swarm.add_agent("b", "receiver")
    msg = swarm.send("a", "b", "", task="test")
    assert msg.sigma is not None
