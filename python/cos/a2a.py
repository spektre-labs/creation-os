# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-A2A v2 — Agent Card, capability negotiation, task lifecycle, σ per channel (lab).

MCP gives tools; this module models **peer agents** (discover / delegate / audit).
No mandatory wire transport — fetch ``/.well-known/agent.json`` when you pass HTTP URLs.
Complements :mod:`cos.sigma_a2a` (hint encoder path). See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import json
import time
import urllib.error
import urllib.request
import uuid
from enum import Enum
from typing import Any, Callable, Dict, List, Mapping, Optional

from cos.sigma_a2a_card import build_sigma_verifier_agent_card

__all__ = ["SigmaA2A", "A2ATaskLifecycle"]


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
