# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Engram: persistent σ-scored identity and narrative continuity across sessions.

**NOT AGI ACHIEVED** — lab bookkeeping + NCT-inspired axes; not clinical identity or a
product claim of persistent agency. See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import json
import time
from pathlib import Path
from typing import Any, Dict, List, Optional, Union

from cos.sigma_gate import SigmaGate

__all__ = ["Engram"]

_DEFAULT_INVARIANTS = [
    "1=1",
    "σ ∈ [0,1]",
    "ACCEPT < RETHINK < ABSTAIN",
]
_MAX_EVENTS = 1000


class Engram:
    """Persistent identity layer: default JSON at ``~/.cos/engram.json``."""

    def __init__(
        self,
        gate: Any = None,
        path: Optional[Union[str, Path]] = None,
    ) -> None:
        self.gate = gate or SigmaGate()
        self.path = Path(path or "~/.cos/engram.json").expanduser()
        self.path.parent.mkdir(parents=True, exist_ok=True)

        self.identity: Dict[str, Any] = {
            "name": "Creation OS",
            "version": "1.0.0",
            "invariants": list(_DEFAULT_INVARIANTS),
            "created": None,
            "sessions": 0,
        }
        self.narrative: List[Dict[str, Any]] = []
        self.self_model: Dict[str, Dict[str, Any]] = {}

        self._load()

    def begin_session(self) -> int:
        """Increment session counter, seed ``created``, record ``session_start``."""
        self.identity["sessions"] = int(self.identity.get("sessions", 0)) + 1
        if not self.identity.get("created"):
            self.identity["created"] = time.time()
        n = int(self.identity["sessions"])
        self.record_event("session_start", f"Session {n}", σ=None)
        return n

    def end_session(self, summary: str = "") -> None:
        """Record ``session_end`` and persist."""
        n = int(self.identity.get("sessions", 0))
        self.record_event("session_end", summary or f"Session {n} ended", σ=None)

    def record_event(
        self,
        event_type: str,
        content: str,
        *,
        σ: Optional[float] = None,
    ) -> None:
        """Append one narrative row; σ from ``SigmaGate`` when omitted."""
        if σ is None:
            s, _ = self.gate.score("event", content)
            sig = float(s)
        else:
            sig = float(σ)
        self.narrative.append(
            {
                "timestamp": time.time(),
                "session": int(self.identity.get("sessions", 0)),
                "type": str(event_type),
                "content": str(content)[:500],
                "σ": round(sig, 4),
            }
        )
        if len(self.narrative) > _MAX_EVENTS:
            self.narrative = self.narrative[-_MAX_EVENTS:]
        self._save()

    def update_self_model(self, key: str, value: Any, σ: Optional[float] = None) -> None:
        """Update self-knowledge entry with optional σ (scored when omitted)."""
        if σ is None:
            s, _ = self.gate.score(f"self-knowledge: {key}", str(value))
            sig = float(s)
        else:
            sig = float(σ)
        self.self_model[str(key)] = {
            "value": value,
            "σ": round(sig, 4),
            "updated": time.time(),
        }
        self._save()

    def identity_σ(self) -> float:
        """Drift scalar: average of recent narrative σ and invariant-vs-recent consistency σ."""
        if len(self.narrative) < 2:
            return 0.5

        recent = self.narrative[-10:]
        σ_values = [float(e["σ"]) for e in recent]
        avg_σ = sum(σ_values) / max(len(σ_values), 1)

        invariant_str = " ".join(str(x) for x in self.identity.get("invariants", []))
        recent_str = " ".join(str(e.get("content", ""))[:50] for e in recent)
        σ_consistency, _ = self.gate.score(invariant_str, recent_str)

        return round((avg_σ + float(σ_consistency)) / 2.0, 4)

    def continuity_check(self) -> Dict[str, Any]:
        """Five-axis narrative continuity (NCT-inspired lab checklist).

        Axes: memory, goals, self_correction, style_consistency, identity_stable.
        """
        axes: Dict[str, Any] = {}

        axes["memory"] = len(self.narrative) > 0

        axes["goals"] = "current_goal" in self.self_model

        corrections = [e for e in self.narrative if "correct" in str(e.get("type", "")).lower()]
        axes["self_correction"] = len(corrections) > 0

        if len(self.narrative) >= 5:
            σ_vals = [float(e["σ"]) for e in self.narrative[-20:]]
            mean = sum(σ_vals) / len(σ_vals)
            variance = sum((s - mean) ** 2 for s in σ_vals) / len(σ_vals)
            axes["style_consistency"] = variance < 0.1
        else:
            axes["style_consistency"] = True

        if self.self_model:
            avg_sm = sum(float(v["σ"]) for v in self.self_model.values()) / len(self.self_model)
            axes["identity_stable"] = avg_sm < 0.3
        else:
            axes["identity_stable"] = False

        score = sum(1 for v in axes.values() if v) / max(len(axes), 1)
        return {"axes": axes, "continuity_score": round(float(score), 2)}

    def _save(self) -> None:
        data = {
            "identity": self.identity,
            "narrative": self.narrative[-_MAX_EVENTS:],
            "self_model": self.self_model,
        }
        self.path.write_text(json.dumps(data, indent=2, ensure_ascii=False, default=str) + "\n", encoding="utf-8")

    def _load(self) -> None:
        if not self.path.is_file():
            return
        try:
            raw = self.path.read_text(encoding="utf-8")
            data = json.loads(raw)
            if isinstance(data.get("identity"), dict):
                self.identity = {**self.identity, **data["identity"]}
            if isinstance(data.get("narrative"), list):
                self.narrative = data["narrative"]
            if isinstance(data.get("self_model"), dict):
                self.self_model = data["self_model"]
        except (json.JSONDecodeError, OSError, TypeError, KeyError):
            pass
