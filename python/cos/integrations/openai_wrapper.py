# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""OpenAI-style client wrapper: run ``chat.completions.create`` then attach ``sigma`` / ``verdict``.

Requires the OpenAI Python SDK only when you call :func:`sigma_chat`::

    pip install openai

Usage::

    from cos.integrations.openai_wrapper import sigma_chat

    response = sigma_chat(client, messages=[{\"role\": \"user\", \"content\": \"Hello\"}])
    print(response.sigma, response.verdict)
"""
from __future__ import annotations

from typing import Any, List, Mapping, MutableMapping, Optional, Sequence

from cos import SigmaGate


def _last_user_content(messages: Sequence[Mapping[str, Any]]) -> str:
    for msg in reversed(list(messages)):
        if str(msg.get("role", "")).lower() == "user":
            c = msg.get("content", "")
            if isinstance(c, list):
                parts: List[str] = []
                for block in c:
                    if isinstance(block, dict):
                        t = block.get("text") or block.get("content")
                        if t is not None:
                            parts.append(str(t))
                    else:
                        parts.append(str(block))
                return "".join(parts)
            return str(c) if c is not None else ""
    return ""


def _choice_text(response: Any) -> str:
    try:
        choice0 = response.choices[0]
        msg = choice0.message
        c = getattr(msg, "content", None)
        return str(c or "")
    except (AttributeError, IndexError, TypeError):
        return ""


def sigma_chat(
    client: Any,
    messages: Sequence[MutableMapping[str, Any]],
    gate: Optional[Any] = None,
    **kwargs: Any,
) -> Any:
    """Call ``client.chat.completions.create`` and attach ``.sigma`` / ``.verdict`` on the response."""
    gate = gate or SigmaGate()
    response = client.chat.completions.create(messages=list(messages), **kwargs)
    prompt = _last_user_content(messages)
    text = _choice_text(response)
    sigma, verdict = gate.score(prompt, text)
    setattr(response, "sigma", float(sigma))
    setattr(response, "verdict", str(verdict))
    return response


__all__ = ["sigma_chat"]
