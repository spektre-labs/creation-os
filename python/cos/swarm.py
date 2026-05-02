# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-swarm — multi-agent coordination with per-message σ-gate validation.

Each agent→agent payload is scored with the same σ-gate used elsewhere in Creation OS.
High-σ or trust-adjusted scores can block propagation before downstream agents consume
bad content (lab pattern; not a substitute for cryptographically anchored identity).

Literature on structured / graph communication motivates treating every hop as an audit
point; this module implements scoring + a simple trust EMA, not external benchmarks.
See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import time
from typing import Any, Callable, Dict, List, Optional

_TRUST_WINDOW = 20


class SwarmAgent:
    """One agent in a :class:`SigmaSwarm` pool."""

    def __init__(
        self,
        name: str,
        role: str,
        fn: Optional[Callable[..., Any]] = None,
    ) -> None:
        self.name = str(name)
        self.role = str(role)
        self.fn = fn
        self.trust_score = 1.0
        self.message_history: List[Dict[str, Any]] = []
        self.sigma_history: List[float] = []

    def update_trust(self, sigma: float) -> None:
        """EMA-style trust: trust ≈ 1 − mean(σ) over the last window of emissions."""
        self.sigma_history.append(float(sigma))
        recent = self.sigma_history[-_TRUST_WINDOW:]
        avg_sigma = sum(recent) / max(len(recent), 1)
        self.trust_score = round(1.0 - avg_sigma, 4)

    def __repr__(self) -> str:
        return f"Agent({self.name!r}, trust={self.trust_score})"


class SwarmMessage:
    """Structured inter-agent message with gate outcome metadata."""

    def __init__(
        self,
        sender: str,
        receiver: str,
        content: str,
        task: Optional[str] = None,
    ) -> None:
        self.sender = str(sender)
        self.receiver = str(receiver)
        self.content = str(content)
        self.task = task
        self.sigma: Optional[float] = None
        self.verdict: Optional[str] = None
        self.timestamp = time.time()
        self.blocked = False

    def __repr__(self) -> str:
        status = "BLOCKED" if self.blocked else f"σ={self.sigma}"
        return f"Msg({self.sender!r}→{self.receiver!r}, {status})"


class SigmaSwarm:
    """Coordinator: validate each relay with ``SigmaGate`` and optional trust weighting."""

    def __init__(self, gate: Any = None, block_threshold: float = 0.85) -> None:
        from cos.sigma_gate import SigmaGate

        self.gate = gate or SigmaGate()
        self.agents: Dict[str, SwarmAgent] = {}
        self.block_threshold = float(block_threshold)
        self.message_log: List[SwarmMessage] = []
        self.execution_history: List[Dict[str, Any]] = []

    def add_agent(
        self,
        name: str,
        role: str,
        fn: Optional[Callable[..., Any]] = None,
    ) -> SwarmAgent:
        agent = SwarmAgent(name, role, fn)
        self.agents[str(name)] = agent
        return agent

    def send(
        self,
        sender_name: str,
        receiver_name: str,
        content: str,
        task: Optional[str] = None,
    ) -> SwarmMessage:
        sender = self.agents.get(sender_name)
        receiver = self.agents.get(receiver_name)
        if not sender or not receiver:
            msg = SwarmMessage(sender_name, receiver_name, content, task)
            msg.blocked = True
            msg.verdict = "AGENT_NOT_FOUND"
            return msg

        msg = SwarmMessage(sender_name, receiver_name, content, task)
        prompt = task or f"Message from {sender_name}"
        sigma, verdict = self.gate.score(prompt, content)
        msg.sigma = round(float(sigma), 4)
        msg.verdict = str(verdict)

        trust_weighted_sigma = float(sigma) / max(sender.trust_score, 0.1)
        if trust_weighted_sigma > self.block_threshold:
            msg.blocked = True
            msg.verdict = "BLOCKED_HIGH_SIGMA"

        sender.update_trust(float(sigma))
        self.message_log.append(msg)

        receiver.message_history.append(
            {
                "from": sender_name,
                "content": content if not msg.blocked else None,
                "sigma": msg.sigma,
                "blocked": msg.blocked,
            }
        )
        return msg

    def execute(
        self,
        task: str,
        pipeline: Optional[List[str]] = None,
    ) -> Dict[str, Any]:
        """Run agents in order; each step’s output is re-scored against ``task``."""
        t0 = time.monotonic()
        agent_order = list(pipeline) if pipeline else list(self.agents.keys())
        context: Any = task
        trace: List[Dict[str, Any]] = []
        blocked = False

        for agent_name in agent_order:
            agent = self.agents.get(agent_name)
            if not agent:
                continue

            if agent.fn is not None:
                response = agent.fn(context, task)
            else:
                response = context

            sigma, verdict = self.gate.score(str(task), str(response))
            agent.update_trust(float(sigma))
            step: Dict[str, Any] = {
                "agent": agent_name,
                "role": agent.role,
                "sigma": round(float(sigma), 4),
                "verdict": str(verdict),
                "trust": agent.trust_score,
                "output_len": len(str(response)),
            }
            trace.append(step)

            if float(sigma) > self.block_threshold:
                blocked = True
                step["blocked"] = True
                break

            context = response

        elapsed = (time.monotonic() - t0) * 1000.0
        result: Dict[str, Any] = {
            "task": task,
            "output": context if not blocked else None,
            "blocked": blocked,
            "trace": trace,
            "agents_used": len(trace),
            "total_agents": len(agent_order),
            "elapsed_ms": round(elapsed, 2),
            "final_sigma": trace[-1]["sigma"] if trace else 1.0,
            "final_verdict": trace[-1]["verdict"] if trace else "NO_AGENTS",
        }
        self.execution_history.append(result)
        return result

    def trust_report(self) -> Dict[str, Dict[str, Any]]:
        return {
            name: {
                "trust": agent.trust_score,
                "messages_sent": len(agent.sigma_history),
                "avg_sigma": sum(agent.sigma_history) / max(len(agent.sigma_history), 1),
                "role": agent.role,
            }
            for name, agent in self.agents.items()
        }

    def cascade_risk(self) -> Dict[str, Any]:
        """Highlight the weakest trust link (lab heuristic for bottleneck triage)."""
        if len(self.agents) < 2:
            return {"risk": 0.0, "bottleneck": None}

        weakest = min(self.agents.values(), key=lambda a: a.trust_score)
        risk = 1.0 - weakest.trust_score
        rec = "ok"
        if risk > 0.7:
            rec = "replace"
        elif risk > 0.3:
            rec = "monitor"
        return {
            "risk": round(risk, 4),
            "bottleneck": weakest.name,
            "bottleneck_trust": weakest.trust_score,
            "recommendation": rec,
        }


__all__ = ["SwarmAgent", "SwarmMessage", "SigmaSwarm"]
