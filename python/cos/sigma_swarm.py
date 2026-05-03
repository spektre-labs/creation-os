# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
σ-governed **multi-agent swarm** scaffold (lab).

**Hallucination Amplification Prevention (HAP):** in unchecked multi-agent graphs, one
agent's bad output can be read, echoed, and "confirmed" by others until informal consensus
drifts from facts.  This module keeps the policy hook small: only outputs that pass
``sigma_gate`` (or a successful RETHINK retry) enter the ``results`` map that downstream
consensus sees — **ABSTAIN** paths do not propagate.

This is **not** a full A2A transport or TLS peer stack; wire your own message envelopes
and persistence.  See ``docs/A2A_COS_TRUST.md`` and ``docs/CLAIM_DISCIPLINE.md``.
"""
from __future__ import annotations

import hashlib
import json
import time
from pathlib import Path
from typing import Any, Dict, List, Mapping, Protocol, Sequence, Tuple

from cos.sigma_twin import TwinGateLab, TwinModelLab

from .sigma_gate_core import Q16, SigmaState, Verdict, sigma_gate


class SwarmAgent(Protocol):
    """Minimal agent surface for :class:`SigmaSwarm`.

    ``retry_with_ttt`` is optional; when missing, :func:`_maybe_retry` re-runs ``execute``.
    """

    id: str

    def execute(self, task: Any) -> str:
        ...

    def get_sigma_state(self) -> SigmaState:
        ...


def _sigma_weight(sigma_q16: int) -> float:
    """Higher weight when σ (Q16 state) is lower — bounded in (0, 1]."""
    s = float(max(0, min(Q16, int(sigma_q16))))
    return max(1e-12, 1.0 - s / float(Q16))


def _maybe_retry(agent: SwarmAgent, task: Any) -> str:
    fn = getattr(agent, "retry_with_ttt", None)
    if callable(fn):
        return str(fn(task))
    return str(agent.execute(task))


class SigmaSwarm:
    """
    Run several agents on the same task; admit only σ-gated outputs into the shared
    ``results`` table; optionally form a σ-weighted consensus over **ACCEPT** branches.
    """

    def __init__(self, agents: Sequence[SwarmAgent]):
        self.agents: List[SwarmAgent] = list(agents)

    def orchestrate(self, task: Any) -> Dict[str, Any]:
        results: Dict[str, Dict[str, Any]] = {}

        for agent in self.agents:
            output = str(agent.execute(task))
            state = agent.get_sigma_state()
            verdict = sigma_gate(state)

            if verdict == Verdict.ACCEPT:
                results[agent.id] = {
                    "output": output,
                    "sigma": int(state.sigma),
                    "verdict": "ACCEPT",
                }
            elif verdict == Verdict.RETHINK:
                retry_out = _maybe_retry(agent, task)
                new_state = agent.get_sigma_state()
                if sigma_gate(new_state) == Verdict.ACCEPT:
                    results[agent.id] = {
                        "output": retry_out,
                        "sigma": int(new_state.sigma),
                        "verdict": "ACCEPT_AFTER_RETRY",
                    }
            # ABSTAIN (and failed retry): omit from results — does not propagate.

        n_total = len(self.agents)
        if len(results) >= 2:
            return self.consensus(results, n_total=n_total)
        if len(results) == 1:
            return {"verdict": "SINGLE_ACCEPT", "payload": next(iter(results.values())), "n_total": n_total}
        return {"verdict": "SWARM_ABSTAIN", "n_total": n_total, "n_accepted": 0}

    def consensus(self, results: Mapping[str, Dict[str, Any]], *, n_total: int) -> Dict[str, Any]:
        """
        σ-weighted selection over accepted agent payloads (not a majority vote).

        Picks the candidate with largest weight ``1 − σ/Q16``; reports normalised share
        of total weight among accepted rows.
        """
        weighted: List[Tuple[str, str, float]] = []
        total_w = 0.0
        for aid, row in results.items():
            w = _sigma_weight(int(row["sigma"]))
            total_w += w
            weighted.append((aid, str(row["output"]), w))

        best_aid, best_out, best_w = max(weighted, key=lambda t: t[2])
        share = float(best_w / total_w) if total_w > 0.0 else 0.0
        return {
            "output": best_out,
            "consensus_weight": share,
            "winner_agent_id": best_aid,
            "n_accepted": len(results),
            "n_total": int(n_total),
            "verdict": "CONSENSUS",
        }


# --- v163 σ-swarm fleet (stigmergy / pheromone lab; distinct from :class:`SigmaSwarm` orchestrate) ---


def _query_hash(prompt: str) -> int:
    return int.from_bytes(hashlib.sha256(prompt.encode("utf-8")).digest()[:4], "big")


class SwarmNodeGate:
    """Wraps :class:`TwinGateLab` with per-node σ bias + running average (specialist routing)."""

    def __init__(self, *, bias: float = 0.0, lab: TwinGateLab | None = None) -> None:
        self.lab = lab or TwinGateLab()
        self.bias = float(bias)
        self._history: List[float] = []

    def score(self, prompt: str, response: str) -> Tuple[float, str]:
        s, v = self.lab.score(prompt, response)
        s = max(0.0, min(1.0, float(s) + self.bias))
        self._history.append(s)
        return s, v

    def avg_sigma(self) -> float:
        if not self._history:
            return 0.5
        return float(sum(self._history) / len(self._history))


class SigmaSwarmNode:
    """One Creation OS-shaped peer: local model + σ trail broadcast to neighbours (lab)."""

    def __init__(self, node_id: str, gate: SwarmNodeGate, model: TwinModelLab) -> None:
        self.id = str(node_id)
        self.gate = gate
        self.model = model
        self.peers: Dict[str, SigmaSwarmNode] = {}
        self.pheromone_trail: Dict[int, Dict[str, Any]] = {}
        self.inbox: List[Dict[str, Any]] = []

    def register_peer(self, peer: SigmaSwarmNode) -> None:
        if peer.id != self.id:
            self.peers[peer.id] = peer

    def broadcast(self, message: Dict[str, Any]) -> None:
        msg = dict(message)
        for peer in self.peers.values():
            peer.receive(msg)

    def query(self, prompt: str) -> Dict[str, Any]:
        response = self.model.generate(prompt)
        sigma, verdict = self.gate.score(prompt, response)
        qh = _query_hash(prompt)
        self.pheromone_trail[qh] = {
            "sigma": sigma,
            "verdict": verdict,
            "timestamp": time.time(),
        }
        self.broadcast(
            {
                "type": "pheromone",
                "from": self.id,
                "query_hash": qh,
                "sigma": sigma,
                "verdict": verdict,
            }
        )
        return {
            "response": response,
            "sigma": sigma,
            "verdict": verdict,
            "node": self.id,
        }

    def receive(self, message: Dict[str, Any]) -> None:
        t = message.get("type")
        if t == "pheromone":
            self.process_pheromone(message)
        elif t == "consensus_request":
            self.inbox.append(message)
        elif t == "help":
            self.inbox.append(message)

    def process_pheromone(self, msg: Dict[str, Any]) -> None:
        qh = int(msg["query_hash"])
        peer_sigma = float(msg["sigma"])
        if qh in self.pheromone_trail:
            own = float(self.pheromone_trail[qh]["sigma"])
            combined = 0.5 * own + 0.5 * peer_sigma
            self.pheromone_trail[qh]["sigma"] = combined
            self.pheromone_trail[qh]["consensus"] = True
        else:
            self.pheromone_trail[qh] = {
                "sigma": peer_sigma,
                "verdict": str(msg.get("verdict", "")),
                "from_peer": str(msg.get("from", "")),
                "timestamp": time.time(),
            }


class SigmaSwarmFleet:
    """N-node mesh with σ as pheromone; no central planner (lab)."""

    def __init__(self) -> None:
        self.nodes: Dict[str, SigmaSwarmNode] = {}

    def add_node(self, node_id: str, *, model: str = "default", bias: float = 0.0) -> SigmaSwarmNode:
        style = model if model in ("default", "short", "verbose") else "default"
        g = SwarmNodeGate(bias=float(bias), lab=TwinGateLab())
        m = TwinModelLab(style=style)
        node = SigmaSwarmNode(node_id, g, m)
        for existing in self.nodes.values():
            node.register_peer(existing)
            existing.register_peer(node)
        self.nodes[node.id] = node
        return node

    def remove_node(self, node_id: str) -> None:
        nid = str(node_id)
        self.nodes.pop(nid, None)
        for p in self.nodes.values():
            p.peers.pop(nid, None)

    def swarm_query(self, prompt: str, strategy: str = "consensus") -> Dict[str, Any]:
        st = str(strategy).lower().strip()
        if not self.nodes:
            return {"verdict": "ABSTAIN", "reason": "empty swarm"}

        if st == "consensus":
            responses: List[Dict[str, Any]] = []
            for node in self.nodes.values():
                responses.append(node.query(prompt))
            best = min(responses, key=lambda r: float(r["sigma"]))
            return {
                "response": best["response"],
                "sigma": best["sigma"],
                "verdict": best["verdict"],
                "selected_node": best["node"],
                "total_nodes": len(responses),
                "all_sigmas": {str(r["node"]): float(r["sigma"]) for r in responses},
                "strategy": "consensus",
            }

        if st == "fastest":
            for node in self.nodes.values():
                result = node.query(prompt)
                if result["verdict"] == "ACCEPT":
                    return {**result, "strategy": "fastest"}
            return {"verdict": "ABSTAIN", "reason": "no node ACCEPT", "strategy": "fastest"}

        if st == "specialist":
            best_node = min(self.nodes.values(), key=lambda n: n.gate.avg_sigma())
            out = best_node.query(prompt)
            return {**out, "strategy": "specialist", "selected_node": best_node.id}

        return {"verdict": "ABSTAIN", "reason": f"unknown strategy {strategy!r}"}

    def health(self) -> Dict[str, Any]:
        n = len(self.nodes)
        avgs = {nid: node.gate.avg_sigma() for nid, node in self.nodes.items()}
        mean = sum(avgs.values()) / float(max(n, 1))
        return {
            "total_nodes": n,
            "avg_sigma": round(mean, 6),
            "node_sigmas": {k: round(v, 6) for k, v in avgs.items()},
        }

    def to_json_obj(self) -> Dict[str, Any]:
        return {
            "version": 1,
            "nodes": [
                {
                    "id": n.id,
                    "model": n.model.style,
                    "bias": n.gate.bias,
                }
                for n in self.nodes.values()
            ],
        }

    @classmethod
    def from_json_obj(cls, obj: Mapping[str, Any]) -> SigmaSwarmFleet:
        fl = cls()
        for row in obj.get("nodes") or []:
            if isinstance(row, dict) and row.get("id"):
                fl.add_node(
                    str(row["id"]),
                    model=str(row.get("model") or "default"),
                    bias=float(row.get("bias") or 0.0),
                )
        return fl

    def save(self, path: Path) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(
            json.dumps(self.to_json_obj(), indent=2, ensure_ascii=False) + "\n",
            encoding="utf-8",
        )

    @classmethod
    def load(cls, path: Path) -> SigmaSwarmFleet:
        raw = json.loads(path.read_text(encoding="utf-8"))
        return cls.from_json_obj(raw)


__all__ = [
    "SigmaSwarm",
    "SigmaSwarmFleet",
    "SigmaSwarmNode",
    "SwarmAgent",
    "SwarmNodeGate",
]
