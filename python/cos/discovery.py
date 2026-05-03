# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-discovery — in-memory agent registry, capability search, σ-ranked match (lab).

Distinct from MCP ``tools/list`` (tool discovery). Pairs with :mod:`cos.a2a`.
See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import json
import time
from typing import Any, Dict, List, Mapping, Optional, Sequence

__all__ = ["SigmaDiscovery"]


class SigmaDiscovery:
    """Register Agent Cards, search by capability string, σ-match tasks to agents."""

    def __init__(self, gate: Any = None) -> None:
        from cos.sigma_gate import SigmaGate

        self.gate = gate or SigmaGate()
        self._agents: Dict[str, Dict[str, Any]] = {}
        self._registered_at: Dict[str, float] = {}

    def register(self, agent_card: Mapping[str, Any]) -> Dict[str, Any]:
        aid = str(agent_card.get("agent_id") or agent_card.get("name") or uuid_agent_id(agent_card))
        self._agents[aid] = dict(agent_card)
        self._registered_at[aid] = time.time()
        return {"agent_id": aid, "registered": True}

    def search(self, capability_query: str) -> List[Dict[str, Any]]:
        q = str(capability_query).lower()
        hits: List[Dict[str, Any]] = []
        for aid, card in self._agents.items():
            caps = card.get("capabilities") or []
            if isinstance(caps, Mapping):
                cap_list = [str(k).lower() for k in caps]
            else:
                cap_list = [str(c).lower() for c in caps]
            blob = " ".join(cap_list)
            if q in blob or any(q in c for c in cap_list):
                hits.append({"agent_id": aid, "card": card})
        return hits

    def sigma_capability_confidence(self, agent_card: Mapping[str, Any]) -> Dict[str, Any]:
        """How much σ stress the advertised capability list induces (lab probe)."""
        caps = agent_card.get("capabilities") or []
        if isinstance(caps, Mapping):
            cap_text = json.dumps(sorted(caps.keys()), sort_keys=True)
        else:
            cap_text = json.dumps([str(c) for c in caps], sort_keys=True)
        sigma, verdict = self.gate.score("discovery_cap_claim", cap_text[:2000])
        conf = max(0.0, 1.0 - float(sigma))
        return {"confidence": round(conf, 6), "sigma": round(float(sigma), 6), "verdict": str(verdict)}

    def stale_detection(self, agent_id: str, *, max_age_s: float = 86400.0) -> Dict[str, Any]:
        """If registration older than ``max_age_s``, flag stale (raise effective σ)."""
        aid = str(agent_id)
        if aid not in self._registered_at:
            return {"stale": True, "effective_sigma": 1.0, "reason": "unknown_agent"}
        age = time.time() - self._registered_at[aid]
        stale = age > float(max_age_s)
        bump = 0.25 if stale else 0.0
        return {"stale": stale, "age_s": round(age, 3), "effective_sigma": round(0.35 + bump, 4)}

    def match(self, task: Mapping[str, Any], available_agents: Sequence[Mapping[str, Any]]) -> Dict[str, Any]:
        """Pick lowest-σ agent that covers required capabilities."""
        required = [str(x).lower() for x in (task.get("required_capabilities") or [])]
        best: Optional[Dict[str, Any]] = None
        best_sigma = 2.0
        for agent in available_agents:
            caps = agent.get("capabilities") or []
            if isinstance(caps, Mapping):
                offer = {str(k).lower() for k in caps}
            else:
                offer = {str(c).lower() for c in caps}
            if not all(r in offer for r in required):
                continue
            blob = json.dumps({"task": dict(task), "agent": agent.get("agent_id")}, sort_keys=True)[:1500]
            sigma, verdict = self.gate.score("discovery_match", blob)
            score = float(sigma) + (0.2 if self.stale_detection(str(agent.get("agent_id", "")))["stale"] else 0.0)
            if score < best_sigma:
                best_sigma = score
                best = {
                    "agent_id": agent.get("agent_id"),
                    "sigma": round(float(sigma), 6),
                    "verdict": str(verdict),
                    "rank_score": round(score, 6),
                }
        return {"best": best, "ok": best is not None}

    def federation(self, registries: Sequence[Mapping[str, Any]]) -> Dict[str, Any]:
        """Merge dicts of ``agent_id -> card`` from multiple registry snapshots."""
        merged: Dict[str, Any] = {}
        for reg in registries:
            for aid, card in reg.items():
                merged[str(aid)] = dict(card) if isinstance(card, Mapping) else card
        return {"agents": merged, "n": len(merged)}


def uuid_agent_id(card: Mapping[str, Any]) -> str:
    import hashlib

    h = hashlib.sha256(json.dumps(dict(card), sort_keys=True, default=str).encode()).hexdigest()[:12]
    return f"anon_{h}"
