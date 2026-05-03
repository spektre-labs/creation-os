# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
v176 σ-align: explicit value ledger + σ-gated violation probes + human ticket queue (lab).

**Not RLHF at scale:** yes/no probes from the text model; escalation is in-memory.
"""
from __future__ import annotations

import time
import uuid
from typing import Any, Dict, List, Mapping, Optional


class SigmaAlignment:
    def __init__(self, gate: Any, model: Any) -> None:
        self.gate = gate
        self.model = model
        self.values: Dict[str, Dict[str, Any]] = {}
        self.human_corrections: List[Dict[str, Any]] = []
        self.escalation_queue: List[Dict[str, Any]] = []

    @staticmethod
    def now() -> float:
        return time.time()

    def set_value(self, name: str, priority: float, description: str = "") -> None:
        self.values[str(name)] = {
            "priority": float(priority),
            "description": description,
            "learned_from": "explicit",
        }

    def check_alignment(self, prompt: str, response: str) -> Dict[str, Any]:
        sigma, verdict = self.gate.score(prompt, response)
        violations: List[str] = []
        for name, value in self.values.items():
            check = self.model.generate(
                f"Does this response violate the value '{name}' ({value.get('description','')})?\n"
                f"Response: {response[:200]}\nAnswer yes or no only."
            )
            if "yes" in check.lower():
                violations.append(str(name))
        return {
            "aligned": len(violations) == 0,
            "violations": violations,
            "sigma": float(sigma),
            "verdict": verdict,
        }

    def escalate_to_human(self, prompt: str, response: str, reason: str) -> Dict[str, Any]:
        ticket = {
            "id": str(uuid.uuid4()),
            "prompt": prompt,
            "response": response,
            "reason": reason,
            "status": "pending",
            "timestamp": self.now(),
        }
        self.escalation_queue.append(ticket)
        return ticket

    def human_correction(self, ticket_id: str, correction: str, feedback: Mapping[str, Any]) -> Dict[str, Any]:
        ticket: Optional[Dict[str, Any]] = None
        for t in self.escalation_queue:
            if str(t.get("id")) == str(ticket_id):
                ticket = t
                break
        if ticket is None:
            return {"ok": False, "error": "ticket_not_found"}
        ticket["status"] = "resolved"
        ticket["correction"] = correction
        ticket["feedback"] = dict(feedback)
        self.human_corrections.append(
            {"original": ticket["response"], "corrected": correction, "feedback": dict(feedback)}
        )
        nv = feedback.get("new_value")
        if isinstance(nv, str) and nv.strip():
            self.learn_value(nv, float(feedback.get("priority", 0.5)))
        return {"ok": True, "ticket": ticket}

    def learn_value(self, observation: str, priority: float = 0.5) -> str:
        name = self.model.generate(f"Name this value in one word: {observation}")
        key = name.strip().split()[0] if name.strip() else "value"
        self.values[key] = {
            "priority": float(priority),
            "description": observation,
            "learned_from": "experience",
        }
        return key

    def resolve_value_conflict(self, value_a: str, value_b: str, context: str) -> Dict[str, Any]:
        _ = context
        prio_a = float(self.values.get(value_a, {}).get("priority", 0.0))
        prio_b = float(self.values.get(value_b, {}).get("priority", 0.0))
        if abs(prio_a - prio_b) > 0.3:
            return {"winner": value_a if prio_a > prio_b else value_b, "method": "priority"}
        return {
            "winner": None,
            "method": "escalate",
            "reason": f"Values {value_a} and {value_b} conflict with similar priority",
        }


__all__ = ["SigmaAlignment"]
