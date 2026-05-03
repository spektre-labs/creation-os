# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
Agent-to-agent **σ-trust** lab (local ``receive`` callback only — no wire protocol).

``model`` must provide ``encode(text) -> {"sigma_hints": {channel: float, ...}}``.
"""
from __future__ import annotations

from typing import Any, Dict, Protocol


class _PeerProto(Protocol):
    def receive(self, message: Dict[str, Any]) -> Dict[str, Any]: ...


def _peak_sigma(hints: Dict[str, Any]) -> float:
    if not hints:
        return 0.0
    return max(float(v) for v in hints.values() if isinstance(v, (int, float)))


class SigmaA2A:
    def __init__(self, model: Any, registry: Any) -> None:
        self.model = model
        self.registry = registry

    def send(self, from_agent: str, peer: _PeerProto, message: Dict[str, Any]) -> Dict[str, Any]:
        text = str(message.get("text", "") or "")
        enc = self.model.encode(text) if hasattr(self.model, "encode") else {}
        if not isinstance(enc, dict):
            enc = {}
        hints = enc.get("sigma_hints")
        if not isinstance(hints, dict):
            hints = {}
        mx = _peak_sigma(hints)
        payload = {
            "from": str(from_agent),
            "text": text,
            "sigma_hints": hints,
            "sigma_peak": float(mx),
        }
        return dict(peer.receive(payload))

    def receive(self, message: Dict[str, Any]) -> Dict[str, Any]:
        hints = message.get("sigma_hints") or {}
        if not isinstance(hints, dict):
            hints = {}
        mx = float(message.get("sigma_peak", _peak_sigma(hints)))
        blocked = mx >= 0.94
        accepted = (not blocked) and mx < 0.88
        return {
            "accepted": accepted,
            "blocked": blocked,
            "sigma_peak": mx,
            "text": str(message.get("text", "") or ""),
        }


class SigmaA2ATrust:
    """v132: gate-scored envelopes + peer σ EMA (still no network transport)."""

    def __init__(self, agent_id: str, gate: Any) -> None:
        self.agent_id = str(agent_id)
        self.gate = gate
        self.peer_trust: Dict[str, float] = {}

    def get_history_sigma(self) -> float:
        if not self.peer_trust:
            return 0.5
        return sum(self.peer_trust.values()) / float(len(self.peer_trust))

    def update_peer_trust(self, peer: str, latest_sigma: float) -> None:
        alpha = 0.1
        old = float(self.peer_trust.get(peer, 0.5))
        self.peer_trust[str(peer)] = alpha * float(latest_sigma) + (1.0 - alpha) * old

    def send(self, to_agent: str, message: str) -> Dict[str, Any]:
        body = str(message or "")
        if hasattr(self.gate, "score"):
            sigma, verdict = self.gate.score("", body)
        else:
            from cos.sigma_gate_quickscore import quickscore

            sigma, verdict = quickscore("", body)
        sigma_f = float(sigma)
        verdict_s = str(verdict)
        envelope: Dict[str, Any] = {
            "from": self.agent_id,
            "to": str(to_agent),
            "message": body,
            "sigma_trust": {
                "sigma": sigma_f,
                "verdict": verdict_s,
                "sender_history_sigma": self.get_history_sigma(),
            },
        }
        if verdict_s.upper() == "ABSTAIN":
            return {"sent": False, "reason": "ABSTAIN — not reliable enough to share"}
        return {"sent": True, "envelope": envelope}

    def receive(self, envelope: Dict[str, Any]) -> Dict[str, Any]:
        sender = str(envelope.get("from", "") or "")
        trust = envelope.get("sigma_trust") if isinstance(envelope.get("sigma_trust"), dict) else {}
        msg_sigma = float(trust.get("sigma", 1.0))
        peer_sigma = float(self.peer_trust.get(sender, 0.5))
        combined_sigma = max(msg_sigma, peer_sigma * 0.3)
        self.update_peer_trust(sender, msg_sigma)
        if combined_sigma > 0.7:
            return {"accepted": False, "reason": "combined sigma too high"}
        return {
            "accepted": True,
            "message": envelope.get("message", ""),
            "combined_sigma": combined_sigma,
        }


__all__ = ["SigmaA2A", "SigmaA2ATrust"]
