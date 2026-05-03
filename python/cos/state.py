# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-state — tiered in-memory state (ephemeral / session / persistent).

σ on each entry is **staleness / trust** metadata (lab). Use with :class:`cos.workflow.SigmaWorkflow`."""
from __future__ import annotations

import time
import uuid
from typing import Any, Dict, Mapping, Optional

__all__ = ["SigmaState"]


class SigmaState:
    """Three-tier store with session isolation and optional σ-weighted merge."""

    TIERS = frozenset({"ephemeral", "session", "persistent"})

    def __init__(self) -> None:
        self._ephemeral: Dict[str, Dict[str, Any]] = {}
        self._session: Dict[str, Dict[str, Dict[str, Any]]] = {}
        self._persistent: Dict[str, Dict[str, Any]] = {}

    @staticmethod
    def _now() -> float:
        return time.monotonic()

    def set(
        self,
        key: str,
        value: Any,
        tier: str,
        sigma: float,
        *,
        session_id: Optional[str] = None,
    ) -> Dict[str, Any]:
        t = str(tier).lower()
        if t not in self.TIERS:
            raise ValueError(f"unknown tier {tier!r}")
        rec = {
            "value": value,
            "sigma": float(sigma),
            "ts": self._now(),
        }
        if t == "ephemeral":
            self._ephemeral[str(key)] = rec
        elif t == "session":
            sid = session_id or str(uuid.uuid4())
            self._session.setdefault(sid, {})[str(key)] = rec
        else:
            self._persistent[str(key)] = rec
        return rec

    def get(
        self,
        key: str,
        tier: str,
        *,
        session_id: Optional[str] = None,
    ) -> Dict[str, Any]:
        t = str(tier).lower()
        if t == "ephemeral":
            r = self._ephemeral.get(str(key))
        elif t == "session":
            if not session_id:
                raise ValueError("session_id required for session tier")
            r = (self._session.get(str(session_id)) or {}).get(str(key))
        else:
            r = self._persistent.get(str(key))
        if not r:
            return {"value": None, "sigma": 1.0, "stale": True, "missing": True}
        eff = self.stale_detection(str(key), tier, session_id=session_id)
        return {**r, **eff}

    def stale_detection(self, key: str, tier: str, *, session_id: Optional[str] = None) -> Dict[str, Any]:
        """Effective σ rises slowly with age (toy half-life)."""
        t = str(tier).lower()
        if t == "ephemeral":
            r = self._ephemeral.get(str(key))
        elif t == "session":
            r = (self._session.get(str(session_id) or "") or {}).get(str(key))
        else:
            r = self._persistent.get(str(key))
        if not r:
            return {"stale": True, "effective_sigma": 1.0}
        age = max(0.0, self._now() - float(r["ts"]))
        half_life_s = {"ephemeral": 30.0, "session": 600.0, "persistent": 86400.0}.get(t, 60.0)
        growth = 1.0 - 0.5 ** (age / max(half_life_s, 1e-6))
        base = float(r["sigma"])
        eff = min(1.0, base + 0.4 * growth)
        return {"stale": eff > 0.75, "effective_sigma": round(eff, 6), "age_s": round(age, 3)}

    def isolation(self, session_id: str) -> str:
        """Return normalized session id (namespace for session tier)."""
        return str(session_id)

    def merge(
        self,
        session_state: Mapping[str, Any],
        persistent_state: Mapping[str, Any],
        *,
        session_id: str,
    ) -> Dict[str, Any]:
        """Prefer lower-σ value per key when both tiers present."""
        out: Dict[str, Any] = {}
        keys = set(session_state.keys()) | set(persistent_state.keys())
        for k in keys:
            s_raw = session_state.get(k)
            p_raw = persistent_state.get(k)
            if s_raw is None:
                out[k] = p_raw
                continue
            if p_raw is None:
                out[k] = s_raw
                continue
            s_sig = float(s_raw.get("sigma", 0.5))
            p_sig = float(p_raw.get("sigma", 0.5))
            out[k] = s_raw if s_sig <= p_sig else p_raw
        return {"merged": out, "session_id": session_id}

    def gc(self, tier: str, max_age_s: float, *, session_id: Optional[str] = None) -> int:
        """Remove entries older than ``max_age_s`` within tier."""
        t = str(tier).lower()
        now = self._now()
        removed = 0
        if t == "ephemeral":
            keys = [k for k, r in self._ephemeral.items() if now - float(r["ts"]) > max_age_s]
            for k in keys:
                del self._ephemeral[k]
                removed += 1
        elif t == "session":
            if not session_id:
                return 0
            bucket = self._session.get(str(session_id)) or {}
            keys = [k for k, r in bucket.items() if now - float(r["ts"]) > max_age_s]
            for k in keys:
                del bucket[k]
                removed += 1
        else:
            keys = [k for k, r in self._persistent.items() if now - float(r["ts"]) > max_age_s]
            for k in keys:
                del self._persistent[k]
                removed += 1
        return removed
