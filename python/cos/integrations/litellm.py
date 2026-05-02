# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""LiteLLM custom logger: score completions and attach ``sigma`` / ``verdict``.

Install::

    pip install 'creation-os[litellm]'

Usage::

    import litellm
    from cos.integrations.litellm import SigmaGateLiteLLM

    litellm.callbacks = [SigmaGateLiteLLM()]
"""
from __future__ import annotations

from typing import Any, Optional

from cos import SigmaGate

try:
    from litellm.integrations.custom_logger import CustomLogger
except ImportError:  # pragma: no cover
    CustomLogger = None  # type: ignore[misc, assignment]


def _last_user_prompt(messages: Any) -> str:
    if not isinstance(messages, list):
        return ""
    for msg in reversed(messages):
        if not isinstance(msg, dict):
            continue
        if str(msg.get("role", "")).lower() == "user":
            c = msg.get("content", "")
            return str(c) if c is not None else ""
    return ""


def _response_text(response_obj: Any) -> str:
    try:
        choices = getattr(response_obj, "choices", None)
        if not choices:
            return ""
        ch0 = choices[0]
        msg = getattr(ch0, "message", None)
        if msg is None:
            return ""
        c = getattr(msg, "content", None)
        return str(c or "")
    except (AttributeError, IndexError, TypeError):
        return ""


if CustomLogger is not None:

    class SigmaGateLiteLLM(CustomLogger):  # type: ignore[misc, valid-type]
        """σ-gate each successful LiteLLM completion."""

        def __init__(self, gate: Any = None) -> None:
            super().__init__()
            self.gate = gate or SigmaGate()
            self.stats = {"total": 0, "accept": 0, "rethink": 0, "abstain": 0}

        def log_success_event(self, kwargs: dict[str, Any], response_obj: Any, start_time: Any, end_time: Any) -> None:  # noqa: ARG002
            del start_time, end_time
            prompt = _last_user_prompt(kwargs.get("messages") or [])
            text = _response_text(response_obj)
            sigma, verdict = self.gate.score(prompt, text)
            setattr(response_obj, "sigma", float(sigma))
            setattr(response_obj, "verdict", str(verdict))
            self.stats["total"] += 1
            b = str(verdict).lower()
            if b in self.stats:
                self.stats[b] += 1

else:  # pragma: no cover

    class SigmaGateLiteLLM:
        def __init__(self, *args: Any, **kwargs: Any) -> None:
            raise ImportError(
                "LiteLLM not installed. Run: pip install 'creation-os[litellm]'"
            )


__all__ = ["SigmaGateLiteLLM"]
