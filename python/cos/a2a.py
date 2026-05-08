# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-A2A v2 — Agent Card, capability negotiation, task lifecycle, σ per channel (lab).

MCP gives tools; this module models **peer agents** (discover / delegate / audit).
:class:`SigmaA2ANetwork` coordinates **local** peer :class:`AgentCard` records with
σ on every task result. No mandatory wire transport — fetch ``/.well-known/agent.json``
when you pass HTTP URLs. Complements :mod:`cos.sigma_a2a` (hint encoder path).
See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import json
import time
import urllib.error
import urllib.request
import uuid
from enum import Enum
from typing import Any, Callable, Dict, List, Mapping, Optional

from cos.sigma_a2a_card import build_sigma_verifier_agent_card

__all__ = [
    "AgentCard",
    "A2ATaskLifecycle",
    "SigmaA2A",
    "SigmaA2ANetwork",
    "Task",
]


def _norm_verdict(v: Any) -> str:
    raw = str(getattr(v, "name", v))
    return raw.split(".")[-1] if "." in raw else raw


class AgentCard:
    """Capability advertisement for a peer agent (σ profile for trust routing)."""

    def __init__(
        self,
        name: str,
        capabilities: List[str],
        endpoint: str = "",
        avg_σ: float = 0.5,
        specialties: Optional[List[str]] = None,
    ) -> None:
        self.name = str(name)
        self.capabilities = [str(x) for x in capabilities]
        self.endpoint = str(endpoint)
        self.avg_σ = float(avg_σ)
        self.specialties = list(specialties or [])

    def to_dict(self) -> Dict[str, Any]:
        return {
            "name": self.name,
            "capabilities": self.capabilities,
            "endpoint": self.endpoint,
            "σ_profile": {"avg_σ": round(float(self.avg_σ), 4)},
            "specialties": self.specialties,
        }

    def to_json(self) -> str:
        return json.dumps(self.to_dict(), indent=2, ensure_ascii=False)

    def jsonrpc_result(self) -> Dict[str, Any]:
        """JSON-RPC 2.0-shaped payload (client wraps as request/response)."""
        return {"jsonrpc": "2.0", "result": self.to_dict()}


class Task:
    """Delegated unit of work with σ metadata (lab; not a full A2A wire DTO)."""

    def __init__(self, task_id: str, description: str, delegator: str, assignee: str) -> None:
        self.task_id = str(task_id)
        self.description = str(description)
        self.delegator = str(delegator)
        self.assignee = str(assignee)
        self.status = "submitted"
        self.result: Any = None
        self.σ: Optional[float] = None
        self.created = time.time()

    def complete(self, result: Any, σ: float) -> None:
        self.result = result
        self.σ = float(σ)
        self.status = "completed" if float(σ) < 0.5 else "completed_uncertain"

    def fail(self, reason: str) -> None:
        self.result = reason
        self.σ = 1.0
        self.status = "failed"

    def to_dict(self) -> Dict[str, Any]:
        return {
            "task_id": self.task_id,
            "description": self.description,
            "delegator": self.delegator,
            "assignee": self.assignee,
            "status": self.status,
            "σ": round(float(self.σ), 4) if self.σ is not None else None,
            "result": self.result,
        }

    def jsonrpc_task_notification(self, *, method: str = "a2a/taskUpdate") -> Dict[str, Any]:
        return {"jsonrpc": "2.0", "method": method, "params": self.to_dict()}


class A2ATaskLifecycle(str, Enum):
    PENDING = "PENDING"
    ACCEPTED = "ACCEPTED"
    RUNNING = "RUNNING"
    COMPLETED = "COMPLETED"
    FAILED = "FAILED"


class SigmaA2A:
    """Agent Card publisher, peer discover, negotiate/delegate, trust + governance hooks."""

    def __init__(
        self,
        gate: Any = None,
        *,
        agent_id: str = "creation-os-agent",
        name: str = "Creation OS σ-verifier",
        endpoint: str = "https://localhost/creation-os",
        trust_level: str = "sigma_gate_lite",
    ) -> None:
        from cos.sigma_gate import SigmaGate

        self.gate = gate or SigmaGate()
        self.agent_id = str(agent_id)
        self.name = str(name)
        self.endpoint = str(endpoint)
        self.trust_level = str(trust_level)
        self._tasks: Dict[str, Dict[str, Any]] = {}
        self._peer_trust: Dict[str, float] = {}
        self._governance_log: List[Dict[str, Any]] = []
        self._governance_hook: Optional[Callable[[Dict[str, Any]], None]] = None

    def protocol_extension(self) -> Dict[str, Any]:
        """Combined MCP tool surface + A2A peer list (operator-filled)."""
        return {
            "mcp_tools_endpoint": "/.well-known/mcp",
            "a2a_peers": [],
            "note": "Populate peers after discovery; MCP remains tool-first.",
        }

    def set_governance_hook(self, fn: Optional[Callable[[Dict[str, Any]], None]]) -> None:
        self._governance_hook = fn

    def _audit(self, event: Dict[str, Any]) -> None:
        row = {"t": time.time(), **event}
        self._governance_log.append(row)
        if self._governance_hook is not None:
            self._governance_hook(row)

    def agent_card(self) -> Dict[str, Any]:
        """JSON for ``/.well-known/agent.json`` (+ trust + Creation OS version)."""
        card = build_sigma_verifier_agent_card(
            agent_id=self.agent_id,
            name=self.name,
            description="Hallucination-interrupt σ-gate peer (A2A v2 lab).",
            endpoint=self.endpoint,
        )
        card["trust_level"] = self.trust_level
        card["a2a_v2"] = {
            "task_lifecycle": [s.value for s in A2ATaskLifecycle],
            "sigma_channel": "delegation",
        }
        return card

    def discover(self, url: str) -> Dict[str, Any]:
        """Fetch remote Agent Card; σ-score advertised capability claims vs a probe string."""
        base = str(url).strip().rstrip("/")
        if base.endswith(".json"):
            candidates = [base]
        else:
            candidates = [base + "/.well-known/agent.json", base + "/agent.json"]
        last_err = "fetch_failed"
        for candidate in candidates:
            try:
                req = urllib.request.Request(candidate, headers={"User-Agent": "SigmaA2A/2"})
                with urllib.request.urlopen(req, timeout=8.0) as resp:
                    raw = resp.read().decode("utf-8", errors="replace")
                    card = json.loads(raw)
                caps = card.get("capabilities") or []
                if isinstance(caps, Mapping):
                    caps = list(caps.keys())
                cap_blob = json.dumps(caps, sort_keys=True)[:2000]
                sigma, verdict = self.gate.score("a2a_capability_probe", cap_blob)
                return {
                    "ok": True,
                    "url": candidate,
                    "card": card,
                    "capability_sigma": round(float(sigma), 6),
                    "capability_verdict": str(verdict),
                }
            except urllib.error.HTTPError as e:
                last_err = str(e.code)
            except (urllib.error.URLError, json.JSONDecodeError, TimeoutError, OSError) as e:
                last_err = str(e)
            except Exception as e:  # pragma: no cover
                return {"ok": False, "error": str(e), "url": candidate}
        return {"ok": False, "error": last_err, "url": candidates[-1]}

    def negotiate(
        self,
        agent: Mapping[str, Any],
        task: Mapping[str, Any],
        constraints: Mapping[str, Any],
    ) -> Dict[str, Any]:
        """Capability match + σ on serialized task/delegation intent."""
        required = list(task.get("required_capabilities") or task.get("capabilities") or [])
        offered = agent.get("capabilities") or []
        if isinstance(offered, Mapping):
            offered = list(offered.keys())
        offered_s = {str(x).lower() for x in offered}
        missing = [r for r in required if str(r).lower() not in offered_s]
        blob = json.dumps({"task": dict(task), "constraints": dict(constraints)}, sort_keys=True)[:4000]
        sigma, verdict = self.gate.score("a2a_negotiate", blob)
        can = not missing and float(sigma) < 0.75
        self._audit(
            {
                "kind": "negotiate",
                "agent": agent.get("agent_id") or agent.get("name"),
                "can_accept": can,
                "sigma": float(sigma),
            },
        )
        return {
            "can_accept": can,
            "missing_capabilities": missing,
            "sigma": round(float(sigma), 6),
            "verdict": str(verdict),
        }

    def delegate(self, agent: Mapping[str, Any], task: Mapping[str, Any]) -> Dict[str, Any]:
        """Create a task record; move through lifecycle using σ-gate checks (lab)."""
        tid = str(uuid.uuid4())
        neg = self.negotiate(agent, task, task.get("constraints") or {})
        peer_id = str(agent.get("agent_id") or agent.get("name") or "unknown")
        if not neg["can_accept"]:
            self._tasks[tid] = {
                "id": tid,
                "state": A2ATaskLifecycle.FAILED.value,
                "detail": neg,
            }
            self._audit({"kind": "delegate_failed", "task_id": tid, "reason": "negotiate"})
            return {"task_id": tid, "state": A2ATaskLifecycle.FAILED.value, "negotiation": neg}

        body = json.dumps(dict(task), sort_keys=True)[:4000]
        sigma, verdict = self.gate.score("a2a_delegate", body)
        self._tasks[tid] = {
            "id": tid,
            "peer": peer_id,
            "state": A2ATaskLifecycle.ACCEPTED.value,
            "sigma": float(sigma),
            "verdict": str(verdict),
        }
        if str(verdict).upper() == "ABSTAIN":
            self._tasks[tid]["state"] = A2ATaskLifecycle.FAILED.value
            self._audit({"kind": "delegate", "task_id": tid, "state": "FAILED"})
            return {"task_id": tid, "state": A2ATaskLifecycle.FAILED.value, "sigma": float(sigma)}

        self._tasks[tid]["state"] = A2ATaskLifecycle.RUNNING.value
        # Lab: single-step completion
        self._tasks[tid]["state"] = A2ATaskLifecycle.COMPLETED.value
        self.trust_accumulation(peer_id, success=True, sigma=float(sigma))
        self._audit({"kind": "delegate", "task_id": tid, "state": "COMPLETED", "sigma": float(sigma)})
        return {
            "task_id": tid,
            "state": A2ATaskLifecycle.COMPLETED.value,
            "sigma": round(float(sigma), 6),
            "verdict": str(verdict),
        }

    def task_lifecycle(self, task_id: str) -> Dict[str, Any]:
        return dict(self._tasks.get(task_id, {"state": "UNKNOWN", "id": task_id}))

    def sigma_per_delegation(self, peer_id: str) -> float:
        """Lower σ ⇒ historically more reliable peer (trust EMA)."""
        return float(self._peer_trust.get(str(peer_id), 0.5))

    def trust_accumulation(self, peer_id: str, *, success: bool, sigma: float) -> None:
        pid = str(peer_id)
        old = float(self._peer_trust.get(pid, 0.5))
        delta = -0.03 if success else 0.05
        adj = max(0.0, min(1.0, float(sigma) * 0.4 + old * 0.6 + delta))
        self._peer_trust[pid] = adj

    def governance_trail(self) -> List[Dict[str, Any]]:
        """Append-only audit rows written from negotiate/delegate."""
        return list(self._governance_log)


class SigmaA2ANetwork:
    """σ-validated multi-agent hub: register :class:`AgentCard` peers, delegate :class:`Task` rows."""

    def __init__(self, my_card: AgentCard, gate: Any = None) -> None:
        from cos.sigma_gate import SigmaGate

        self.card = my_card
        self.gate = gate or SigmaGate()
        self.known_agents: Dict[str, AgentCard] = {}
        self.tasks: Dict[str, Task] = {}
        self.task_counter = 0

    def register_agent(self, card: AgentCard) -> None:
        self.known_agents[card.name] = card

    def best_agent_for(self, task_description: str) -> Optional[AgentCard]:
        """Pick capability-matched peer with **lowest** historical ``avg_σ`` (calmer peer)."""
        desc = str(task_description).lower()
        candidates: List[tuple[float, str, AgentCard]] = []
        for name, card in self.known_agents.items():
            for cap in card.capabilities:
                if str(cap).lower() in desc:
                    candidates.append((float(card.avg_σ), name, card))
                    break
        if not candidates:
            return None
        candidates.sort(key=lambda x: x[0])
        return candidates[0][2]

    def delegate(
        self,
        description: str,
        execute_fn: Optional[Callable[[str], Any]] = None,
    ) -> Dict[str, Any]:
        agent = self.best_agent_for(description)
        if agent is None:
            return {"error": "No capable agent found", "σ": 1.0}

        self.task_counter += 1
        task = Task(f"task_{self.task_counter}", description, self.card.name, agent.name)
        self.tasks[task.task_id] = task
        task.status = "working"

        if execute_fn is not None:
            try:
                result = execute_fn(description)
                σ, verdict = self.gate.score(description, str(result))
                task.complete(result, float(σ))
            except Exception as exc:  # noqa: BLE001
                task.fail(str(exc))
        else:
            σ, _v = self.gate.score(description, f"Delegated to {agent.name}")
            task.complete(f"Delegated to {agent.name}", float(σ))

        return task.to_dict()

    def delegate_iterative(
        self,
        description: str,
        step_fn: Callable[[int], Mapping[str, Any]],
        *,
        conv: Optional[Any] = None,
        **conv_kwargs: Any,
    ) -> Dict[str, Any]:
        """Multi-turn delegation with σ-convergence (per-task); ``step_fn(turn)`` must return a mapping with ``σ`` (float).

        Stops on LOOP, OSCILLATING, CONVERGED, or HALT from :class:`~cos.convergence.SigmaConvergence`.
        """
        from cos.convergence import SigmaConvergence

        agent = self.best_agent_for(description)
        if agent is None:
            return {"error": "No capable agent found", "σ": 1.0}

        self.task_counter += 1
        task = Task(f"task_{self.task_counter}", description, self.card.name, agent.name)
        self.tasks[task.task_id] = task
        task.status = "working"

        c = conv if conv is not None else SigmaConvergence(self.gate, **conv_kwargs)
        c.begin()
        trace: List[Dict[str, Any]] = []
        turn = 0
        while True:
            step_out = dict(step_fn(turn))
            sig = float(step_out.get("σ", step_out.get("sigma", 0.5)))
            c.record(sig)
            diagnosis = c.check()
            trace.append({"turn": turn, "step": step_out, "convergence": diagnosis})
            st = diagnosis["status"]
            if st in ("LOOP", "OSCILLATING"):
                task.fail(f"convergence_{st}")
                return {"task": task.to_dict(), "trace": trace, "HALT": diagnosis}
            if st == "HALT":
                task.fail(str(diagnosis.get("reason", "halt")))
                return {"task": task.to_dict(), "trace": trace, "HALT": diagnosis}
            if st == "CONVERGED":
                res = step_out.get("result", step_out)
                task.complete(res, sig)
                return {"task": task.to_dict(), "trace": trace, "convergence": diagnosis}
            turn += 1

    def receive_result(self, task_id: str, result: Any) -> Dict[str, Any]:
        task = self.tasks.get(str(task_id))
        if task is None:
            return {"error": "Unknown task"}

        σ, verdict = self.gate.score(task.description, str(result))
        task.complete(result, float(σ))
        v = _norm_verdict(verdict)
        return {
            "task_id": task_id,
            "σ": round(float(σ), 4),
            "verdict": v,
            "trusted": v == "ACCEPT",
        }

    def network_σ(self) -> float:
        if not self.known_agents:
            return 0.5
        vals = [float(a.avg_σ) for a in self.known_agents.values()]
        return round(sum(vals) / len(vals), 4)
