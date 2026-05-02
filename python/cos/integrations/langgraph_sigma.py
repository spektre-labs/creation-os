# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
"""σ-gate helpers for LangGraph-style state machines (optional ``langgraph``).

Install::

    pip install 'creation-os[langgraph]'

This module avoids importing ``langgraph`` at import time so ``import cos`` stays
light when the extra is not installed.

Typical pattern::

    from cos.integrations.langgraph_sigma import sigma_gate_node, sigma_gate_router

    graph.add_node(\"sigma_gate\", lambda s: sigma_gate_node(s))
    graph.add_conditional_edges(\"sigma_gate\", sigma_gate_router)
"""
from __future__ import annotations

from typing import Any, Dict, List


def _message_content(msg: Any) -> str:
    if msg is None:
        return ""
    c = getattr(msg, "content", msg)
    return str(c) if c is not None else ""


def sigma_gate_node(
    state: Dict[str, Any],
    gate: Any = None,
    *,
    append_abstain_system_note: bool = False,
) -> Dict[str, Any]:
    """Attach ``sigma`` / ``verdict`` from the last message pair; optional ABSTAIN system note.

    When ``append_abstain_system_note`` is True and ``verdict == "ABSTAIN"``, appends a plain
    ``dict`` ``{"role": "system", "content": ...}`` if messages are a ``list`` of ``dict``s
    (mixed LangChain message types are left unchanged except for fields on ``out``).
    """
    from cos.scoring import default_scorer

    out = dict(state)
    messages: List[Any] = list(out.get("messages") or [])
    if not messages:
        out["sigma"] = 0.0
        out["verdict"] = "ACCEPT"
        return out

    last_message = messages[-1]
    response = _message_content(last_message)
    prompt_msg = messages[-2] if len(messages) > 1 else None
    prompt = _message_content(prompt_msg)

    scorer = default_scorer(gate)
    sigma, verdict = scorer.score(prompt, response)
    out["sigma"] = sigma
    out["verdict"] = verdict

    if append_abstain_system_note and verdict == "ABSTAIN":
        msgs = list(out.get("messages") or [])
        if msgs and all(isinstance(m, dict) for m in msgs):
            msgs = msgs + [
                {
                    "role": "system",
                    "content": (
                        f"σ-gate: ABSTAIN (σ={sigma:.3f}). "
                        "Last model response flagged as unreliable."
                    ),
                }
            ]
            out["messages"] = msgs
    return out


def sigma_gate_router(state: Dict[str, Any]) -> str:
    """Route on ``verdict`` → ``output`` | ``regenerate`` | ``abstain``."""
    verdict = str(state.get("verdict", "ACCEPT"))
    if verdict == "ACCEPT":
        return "output"
    if verdict == "RETHINK":
        return "regenerate"
    return "abstain"


def require_langgraph() -> None:
    """Raise ImportError if ``langgraph`` is not installed."""
    try:
        import langgraph  # noqa: F401
    except ImportError as e:  # pragma: no cover
        raise ImportError(
            "langgraph is required for compiled graphs. "
            "Install with: pip install 'creation-os[langgraph]'"
        ) from e


__all__ = ["sigma_gate_node", "sigma_gate_router", "require_langgraph"]
