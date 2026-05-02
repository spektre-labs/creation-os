# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
"""AutoGen-style σ hook for message lists (optional ``autogen-agentchat``).

Mutates the last message in place with ``sigma`` / ``verdict`` and optional
content suffix on ABSTAIN.
Wire where your group chat exposes ``messages`` (dicts with ``content`` keys).

Install (for upstream AutoGen types)::

    pip install 'creation-os[autogen]'
"""
from __future__ import annotations

from typing import Any, Dict, List, Optional

from cos.scoring import default_scorer


class SigmaAutoGenHook:
    """σ-check on the last message using the preceding message as prompt context."""

    def __init__(self, gate: Any = None) -> None:
        self.gate = gate
        self._scorer = default_scorer(gate)

    def process_last_received_message(
        self,
        messages: Optional[List[Dict[str, Any]]],
        sender: Any = None,
    ) -> Optional[List[Dict[str, Any]]]:
        del sender
        if not messages:
            return messages
        last = messages[-1]
        if not isinstance(last, dict):
            return messages
        content = str(last.get("content", ""))
        prompt = ""
        if len(messages) > 1:
            prev = messages[-2]
            if isinstance(prev, dict):
                prompt = str(prev.get("content", ""))

        sigma, verdict = self._scorer.score(prompt, content)
        last["sigma"] = sigma
        last["verdict"] = verdict
        if verdict == "ABSTAIN":
            suffix = (
                f"\n\n[σ-gate WARNING: σ={sigma:.3f}, verdict=ABSTAIN — "
                "response may be unreliable]"
            )
            last["content"] = content + suffix
        return messages


class SigmaGateHook(SigmaAutoGenHook):
    """Legacy alias: single-message pipeline (prompt context not from thread)."""

    def process_message(
        self,
        sender: Any,
        receiver: Any,
        message: Dict[str, Any],
    ) -> Dict[str, Any]:
        """Score ``message`` content alone (empty prompt context)."""
        del sender, receiver
        from copy import deepcopy

        out = deepcopy(message)
        content = str(out.get("content", ""))
        sigma, verdict = self._scorer.score("", content)
        out["sigma"] = sigma
        out["verdict"] = verdict
        if verdict == "ABSTAIN":
            out["content"] = f"[σ-ABSTAIN: original message not reliable, σ={sigma:.3f}]"
        return out


__all__ = ["SigmaAutoGenHook", "SigmaGateHook"]
