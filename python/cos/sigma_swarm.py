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

from typing import Any, Dict, List, Mapping, Protocol, Sequence, Tuple

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


__all__ = ["SigmaSwarm", "SwarmAgent"]
