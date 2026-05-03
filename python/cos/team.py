# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Multi-role σ-team orchestration (lab) — analyst / critic / writer / verifier + debate.

Aligns with σ-swarm / fleet / conflict modules as a **structured** subset; not a live MCP fleet.
"""
from __future__ import annotations

import statistics
from typing import Any, Callable, Dict, Mapping, MutableMapping, Optional, Sequence, Tuple

__all__ = ["SigmaTeam"]

Role = str
ResponseFn = Callable[[str], str]


class SigmaTeam:
    """Route tasks to roles; pick answers by lowest σ; optional maintainer override."""

    ROLES: Tuple[str, ...] = ("analyst", "critic", "writer", "verifier")

    def __init__(
        self,
        gate: Any,
        *,
        responders: Optional[Mapping[str, ResponseFn]] = None,
        proconductor_override: bool = False,
    ) -> None:
        self.gate = gate
        self.proconductor_override = bool(proconductor_override)
        self._responders: MutableMapping[str, ResponseFn] = dict(responders or {})

    def register(self, role: str, fn: ResponseFn) -> None:
        self._responders[str(role)] = fn

    def assign(self, task: str, roles: Sequence[str]) -> Dict[str, str]:
        out: Dict[str, str] = {}
        for r in roles:
            out[str(r)] = f"{r}: handle → {task[:240]}"
        return out

    def round_robin(self, task: str, team: Sequence[str]) -> Dict[str, Any]:
        responses: Dict[str, str] = {}
        scores: Dict[str, float] = {}
        for role in team:
            fn = self._responders.get(str(role))
            text = fn(task) if callable(fn) else f"[{role}] {task[:120]}"
            responses[str(role)] = text
            sigma, _verdict = self.gate.score(task, text)
            scores[str(role)] = float(sigma)
        if self.proconductor_override:
            first = str(team[0])
            picked = first
            pick_sigma = scores[first]
        else:
            picked = min(scores, key=scores.get)  # type: ignore[arg-type]
            pick_sigma = scores[picked]
        return {"picked": picked, "sigma": pick_sigma, "responses": responses, "scores": scores}

    def debate(
        self,
        question: str,
        pro_model: ResponseFn,
        con_model: ResponseFn,
    ) -> Dict[str, Any]:
        pro_text = pro_model(question)
        con_text = con_model(question)
        s_p, v_p = self.gate.score(question, pro_text)
        s_c, v_c = self.gate.score(question, con_text)
        if s_p < s_c:
            winner, reason = "pro", "lower_sigma"
        elif s_c < s_p:
            winner, reason = "con", "lower_sigma"
        else:
            winner, reason = "tie", "equal_sigma"
        return {
            "pro": {"text": pro_text, "sigma": float(s_p), "verdict": v_p},
            "con": {"text": con_text, "sigma": float(s_c), "verdict": v_c},
            "winner": winner,
            "reason": reason,
        }

    @staticmethod
    def sigma_consensus(responses: Mapping[str, float]) -> Dict[str, Any]:
        """Given σ per role response — low stdev ⇒ agreement under gate stress."""
        vals = list(responses.values())
        if not vals:
            return {"consensus": True, "stdev": 0.0, "mean": 0.0}
        st = statistics.pstdev(vals) if len(vals) > 1 else 0.0
        mu = sum(vals) / len(vals)
        return {"consensus": st < 0.12, "stdev": round(st, 6), "mean": round(mu, 6)}
