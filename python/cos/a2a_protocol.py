# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
A2A-shaped **task lifecycle** and delegation stubs (v123).

Agent Cards remain canonical in ``sigma_a2a_card``. This module adds Linux-Foundation-style
state names for lab JSON without implying full Google A2A transport compliance.
"""
from __future__ import annotations

from cos.sigma_gate import SigmaGate  # noqa: F401 — σ kernel anchor (sigma_gate.h / Python lite)

from enum import Enum
from typing import Any, Dict

from cos.mcp_marketplace import default_agent_card
from cos.sigma_a2a import SigmaA2A
from cos.sigma_mcp_registry import SigmaMCPRegistry


class A2ATaskState(str, Enum):
    working = "working"
    input_required = "input-required"
    completed = "completed"
    failed = "failed"


def build_agent_card_json(endpoint: str = "https://localhost/creation-os") -> Dict[str, Any]:
    card = default_agent_card(endpoint=endpoint)
    card["a2a"] = {"version": "1.2-lab", "task_states": [s.value for s in A2ATaskState]}
    return card


def delegate_task_lab(
    *,
    registry: SigmaMCPRegistry,
    from_agent: str,
    to_agent: str,
    task_text: str,
    model: Any,
) -> Dict[str, Any]:
    """
    Run σ-trust send/receive path; map result to simplified task lifecycle JSON.
    """
    a2a = SigmaA2A(model=model, registry=registry)

    class _Peer:
        def receive(self, message: Dict[str, Any]) -> Dict[str, Any]:
            return a2a.receive(message)

    out = a2a.send(from_agent, _Peer(), {"text": task_text})
    if out.get("blocked"):
        return {"state": A2ATaskState.failed.value, "detail": out}
    if out.get("accepted") is False:
        return {"state": A2ATaskState.input_required.value, "detail": out}
    return {"state": A2ATaskState.completed.value, "detail": out}


__all__ = ["A2ATaskState", "build_agent_card_json", "delegate_task_lab"]
