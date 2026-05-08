# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-session — per-user chat/workflow sessions with σ history (in-memory lab)."""
from __future__ import annotations

from cos.sigma_gate import SigmaGate  # noqa: F401 — σ kernel anchor (sigma_gate.h / Python lite)

import statistics
import time
import uuid
from typing import Any, Dict, List, Optional

__all__ = ["SigmaSession"]


class SigmaSession:
    """Track turns, σ per query, context window; optional handoff between agents."""

    def __init__(self) -> None:
        self._sessions: Dict[str, Dict[str, Any]] = {}

    def create(self, user_id: str, persona: Optional[str] = None) -> Dict[str, Any]:
        sid = str(uuid.uuid4())
        self._sessions[sid] = {
            "user_id": str(user_id),
            "persona": str(persona) if persona else None,
            "created": time.time(),
            "messages": [],  # {role, content, sigma}
            "agent": "default",
            "cost_eur_accum": 0.0,
        }
        return {"session_id": sid, "user_id": str(user_id), "persona": persona}

    def sigma_history(self, session_id: str) -> List[Dict[str, Any]]:
        s = self._sessions.get(str(session_id))
        if not s:
            return []
        return [{"sigma": m.get("sigma"), "role": m.get("role")} for m in s["messages"] if "sigma" in m]

    def append_turn(
        self,
        session_id: str,
        *,
        role: str,
        content: str,
        sigma: Optional[float] = None,
    ) -> None:
        s = self._sessions.get(str(session_id))
        if not s:
            return
        s["messages"].append({"role": role, "content": content, "sigma": sigma, "ts": time.time()})

    def context_window(self, session_id: str, max_tokens: int) -> List[Dict[str, Any]]:
        """Crude token budget: ~4 chars per token."""
        s = self._sessions.get(str(session_id))
        if not s:
            return []
        budget = max(1, int(max_tokens)) * 4
        out: List[Dict[str, Any]] = []
        used = 0
        for m in reversed(s["messages"]):
            chunk = str(m.get("content", ""))
            room = budget - used
            if room <= 0:
                break
            if len(chunk) > room:
                chunk = chunk[-room:]
            out.append({"role": m["role"], "content": chunk, "sigma": m.get("sigma")})
            used += len(chunk)
        return list(reversed(out))

    def session_sigma_trend(self, session_id: str) -> Dict[str, Any]:
        sigs = [float(m["sigma"]) for m in self._sessions.get(str(session_id), {}).get("messages", []) if m.get("sigma") is not None]
        if len(sigs) < 2:
            return {"rising": False, "note": "insufficient_data"}
        rising = sigs[-1] > sigs[0] + 0.02
        return {
            "rising": rising,
            "first": round(sigs[0], 4),
            "last": round(sigs[-1], 4),
            "delta": round(sigs[-1] - sigs[0], 6),
        }

    def cleanup(self, max_age_minutes: float) -> int:
        cutoff = time.time() - float(max_age_minutes) * 60.0
        dead = [sid for sid, s in self._sessions.items() if float(s["created"]) < cutoff]
        for sid in dead:
            del self._sessions[sid]
        return len(dead)

    def multi_turn_sigma(self, session_id: str) -> Dict[str, Any]:
        sigs = [float(m["sigma"]) for m in self._sessions.get(str(session_id), {}).get("messages", []) if m.get("sigma") is not None]
        prod = 1.0
        for s in sigs:
            prod *= max(0.0, min(1.0, 1.0 - s))
        return {
            "n_sigma_turns": len(sigs),
            "cumulative_reliability_proxy": round(prod, 6),
            "mean_sigma": round(float(statistics.mean(sigs)), 6) if sigs else 0.0,
        }

    def handoff(self, session_id: str, new_agent: str) -> Dict[str, Any]:
        s = self._sessions.get(str(session_id))
        if not s:
            return {"ok": False, "error": "session_not_found"}
        old = str(s.get("agent", ""))
        s["agent"] = str(new_agent)
        s["messages"].append({"role": "system", "content": f"handoff {old!r} -> {new_agent!r}", "ts": time.time()})
        return {"ok": True, "from": old, "to": str(new_agent)}
