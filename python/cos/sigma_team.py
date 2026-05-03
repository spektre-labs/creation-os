# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
cos team — multi-user σ-workspace (shared engram, RBAC, σ-budget, audit).

Each query is σ-scored and logged. Only **ACCEPT** rows with **write** permission hit Engram.

Does not modify ``sigma_gate.h``. EU AI Act / operational evidence: treat exports as **local
receipts**; see ``docs/CLAIM_DISCIPLINE.md``.
"""
from __future__ import annotations

import time
from datetime import datetime, timezone
from typing import Any, Callable, Dict, List, Optional, Protocol, Tuple, Union, runtime_checkable

from cos.sigma_gate_core import Verdict
from cos.sigma_rbac import SigmaRBAC

VerdictLike = Union[str, Verdict]


def _verdict_label(v: VerdictLike) -> str:
    if isinstance(v, Verdict):
        return v.name
    s = str(v).strip().upper()
    for name in ("ACCEPT", "RETHINK", "ABSTAIN"):
        if name in s:
            return name
    return "RETHINK"


@runtime_checkable
class TeamGate(Protocol):
    def score(self, prompt: str, response: str) -> Tuple[float, VerdictLike]:
        ...


@runtime_checkable
class TeamModel(Protocol):
    def generate(self, prompt: str, *, context: Optional[List[Any]] = None) -> str:
        ...


@runtime_checkable
class TeamEngramLike(Protocol):
    def recall(self, query: str, tau: float = 0.3, max_results: int = 10) -> List[Dict[str, Any]]:
        ...

    def store(
        self,
        content: str,
        source: str,
        sigma: float,
        verdict: VerdictLike,
        *,
        memory_type: str = "episodic",
    ) -> Dict[str, Any]:
        ...


class SigmaTeam:
    """Shared σ-gated agent surface: members, audit trail, optional RBAC budgets."""

    def __init__(
        self,
        gate: TeamGate,
        engram: TeamEngramLike,
        *,
        model: Optional[TeamModel] = None,
        rbac: Optional[SigmaRBAC] = None,
        now_fn: Optional[Callable[[], float]] = None,
        shared_context_limit: int = 256,
    ) -> None:
        self.gate = gate
        self.engram = engram
        self.model = model
        self.rbac = rbac or SigmaRBAC()
        self._now = now_fn or time.time
        self.shared_context_limit = max(32, int(shared_context_limit))
        self.members: Dict[str, Dict[str, Any]] = {}
        self.shared_context: List[Dict[str, Any]] = []
        self.audit_log: List[Dict[str, Any]] = []

    @staticmethod
    def now_iso(ts: Optional[float] = None) -> str:
        t = time.time() if ts is None else float(ts)
        return datetime.fromtimestamp(t, tz=timezone.utc).isoformat()

    def _ts(self) -> float:
        return float(self._now())

    def add_member(self, user_id: str, role: str = "member", permissions: Optional[List[str]] = None) -> None:
        self.members[str(user_id)] = self.rbac.member_record(role, permissions=permissions)

    def get_shared_context(self, prompt: str) -> List[Any]:
        engram_results = list(self.engram.recall(str(prompt), tau=0.3, max_results=10))
        recent_queries = list(self.audit_log[-20:])
        tail_shared = list(self.shared_context[-20:])
        return engram_results + tail_shared + recent_queries

    def query(self, user_id: str, prompt: str) -> Dict[str, Any]:
        uid = str(user_id)
        member = self.members.get(uid)
        if not member:
            return {"error": "not a team member"}

        if not self.rbac.check_budget(member):
            return {"error": "daily σ-budget exhausted"}

        if not self.rbac.check_permission(member, "query"):
            return {"error": "permission denied: query"}

        if self.model is None:
            return {"error": "no model configured"}

        context = self.get_shared_context(prompt)
        response = self.model.generate(prompt, context=context)
        sigma, verdict = self.gate.score(prompt, response)
        vn = _verdict_label(verdict)

        entry = {
            "user_id": uid,
            "prompt": str(prompt)[:200],
            "sigma": float(sigma),
            "verdict": vn,
            "timestamp": self.now_iso(self._ts()),
            "ts": self._ts(),
        }
        self.audit_log.append(entry)

        wrote = False
        if vn == "ACCEPT" and self.rbac.check_permission(member, "write"):
            self.engram.store(
                str(response),
                source=f"team:{uid}",
                sigma=float(sigma),
                verdict=Verdict.ACCEPT,
                memory_type="episodic",
            )
            wrote = True
            self.shared_context.append(
                {
                    "content": str(response)[:800],
                    "user_id": uid,
                    "sigma": float(sigma),
                    "timestamp": entry["timestamp"],
                }
            )
            while len(self.shared_context) > self.shared_context_limit:
                self.shared_context.pop(0)

        member["used"] = int(member.get("used", 0)) + 1

        return {
            "response": response,
            "sigma": float(sigma),
            "verdict": vn,
            "context_used": len(context),
            "queried_by": uid,
            "stored_shared_memory": wrote,
        }

    def top_users_by_usage(self, limit: int = 10) -> List[Dict[str, Any]]:
        counts: Dict[str, int] = {}
        for a in self.audit_log:
            u = str(a.get("user_id", ""))
            if u:
                counts[u] = counts.get(u, 0) + 1
        ranked = sorted(counts.items(), key=lambda x: -x[1])[: max(1, int(limit))]
        return [{"user_id": u, "queries": n} for u, n in ranked]

    def sigma_trend(self, window: int = 20) -> List[Dict[str, Any]]:
        w = max(1, int(window))
        tail = self.audit_log[-w:]
        return [{"sigma": float(a.get("sigma", 0)), "verdict": a.get("verdict"), "user_id": a.get("user_id")} for a in tail]

    def team_dashboard(self) -> Dict[str, Any]:
        n = len(self.audit_log)
        avg_sigma = sum(float(a.get("sigma", 0)) for a in self.audit_log) / max(n, 1)
        return {
            "members": len(self.members),
            "total_queries": n,
            "avg_sigma": avg_sigma,
            "verdict_distribution": {
                "ACCEPT": sum(1 for a in self.audit_log if str(a.get("verdict")) == "ACCEPT"),
                "RETHINK": sum(1 for a in self.audit_log if str(a.get("verdict")) == "RETHINK"),
                "ABSTAIN": sum(1 for a in self.audit_log if str(a.get("verdict")) == "ABSTAIN"),
            },
            "top_users": self.top_users_by_usage(),
            "sigma_trend": self.sigma_trend(),
        }

    def budget_status(self) -> List[Dict[str, Any]]:
        rows: List[Dict[str, Any]] = []
        for uid, m in sorted(self.members.items()):
            role = str(m.get("role", "viewer"))
            limit = int(m.get("sigma_budget", 0))
            used = int(m.get("used", 0))
            if limit < 0:
                rows.append({"user_id": uid, "role": role, "used": used, "limit": "unlimited"})
            else:
                rows.append({"user_id": uid, "role": role, "used": used, "limit": limit, "remaining": max(0, limit - used)})
        return rows

    def export_audit(self, *, export_format: str = "json") -> Dict[str, Any]:
        _ = export_format
        return {
            "audit_log": list(self.audit_log),
            "members": dict(self.members),
            "team_size": len(self.members),
            "total_verdicts": len(self.audit_log),
            "export_timestamp": self.now_iso(self._ts()),
            "gate_reference": "sigma_gate.h (canonical kernel — unchanged by sigma_team lab)",
            "license": "LicenseRef-SCSL-1.0 OR AGPL-3.0-only",
        }

    def to_state(self) -> Dict[str, Any]:
        return {
            "members": dict(self.members),
            "audit_log": list(self.audit_log),
            "shared_context": list(self.shared_context),
        }

    @classmethod
    def from_state(cls, blob: Dict[str, Any], gate: TeamGate, engram: TeamEngramLike, **kwargs: Any) -> "SigmaTeam":
        obj = cls(gate, engram, **kwargs)
        obj.members = dict(blob.get("members") or {})
        obj.audit_log = list(blob.get("audit_log") or [])
        obj.shared_context = list(blob.get("shared_context") or [])
        return obj


__all__ = ["SigmaTeam"]
