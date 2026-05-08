# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""LiteLLM completion helpers with σ-gate (any LiteLLM-supported model).

Requires LiteLLM::

    pip install 'creation-os[litellm]'
"""
from __future__ import annotations

from typing import Any, Dict, List, Optional

from cos import SigmaGate
from cos.integrations.litellm import _last_user_prompt, _response_text

_default_gate: Optional[SigmaGate] = None


def _get_gate() -> SigmaGate:
    global _default_gate
    if _default_gate is None:
        _default_gate = SigmaGate()
    return _default_gate


def sigma_completion(
    prompt: str,
    model: str = "gpt-4o",
    gate: Any = None,
    **kwargs: Any,
) -> Dict[str, Any]:
    """Run ``litellm.completion`` and attach ``response``, ``sigma``, ``verdict``."""
    try:
        import litellm
    except ImportError as e:
        raise ImportError("pip install litellm") from e

    g = gate or _get_gate()
    messages: List[Dict[str, Any]] = list(
        kwargs.pop("messages", None) or [{"role": "user", "content": prompt}]
    )
    response = litellm.completion(model=model, messages=messages, **kwargs)
    text = _response_text(response)
    user_prompt = _last_user_prompt(messages) or prompt
    sigma, verdict = g.score(user_prompt, text)
    return {
        "response": text,
        "sigma": float(sigma),
        "verdict": str(verdict),
        "model": model,
        "raw": response,
    }


def sigma_completion_with_fallback(
    prompt: str,
    models: List[str],
    gate: Any = None,
    **kwargs: Any,
) -> Dict[str, Any]:
    """Try ``models`` in order; stop early on ACCEPT."""
    g = gate or _get_gate()
    last: Dict[str, Any] = {}
    for model in models:
        last = sigma_completion(prompt, model=model, gate=g, **kwargs)
        if last.get("verdict") == "ACCEPT":
            return last
    return last


def _latency_ms(start: Any, end: Any) -> float:
    try:
        delta = end - start
        if hasattr(delta, "total_seconds"):
            return float(delta.total_seconds()) * 1000.0
        return float(delta) * 1000.0
    except Exception:
        return 0.0


class SigmaLiteLLMCallback:
    """LiteLLM-style callback: σ-gate on successful completions (no ``litellm`` import required)."""

    def __init__(self, gate: Any = None) -> None:
        self.gate = gate or _get_gate()
        self.trace: List[Dict[str, Any]] = []

    def log_success_event(  # noqa: PLR0913
        self,
        kwargs: Any,
        response_obj: Any,
        start_time: Any,
        end_time: Any,
    ) -> None:
        messages = list(kwargs.get("messages") or [])
        user_prompt = _last_user_prompt(messages)
        if not user_prompt and messages:
            last = messages[-1]
            if isinstance(last, dict):
                user_prompt = str(last.get("content") or "")
            else:
                c = getattr(last, "content", last)
                user_prompt = str(c) if c is not None else ""
        text = _response_text(response_obj)
        sigma, verdict = self.gate.score(user_prompt, text)
        self.trace.append(
            {
                "model": str(kwargs.get("model", "unknown")),
                "sigma": round(float(sigma), 4),
                "verdict": str(verdict),
                "latency_ms": round(_latency_ms(start_time, end_time), 1),
            }
        )

    def log_failure_event(  # noqa: PLR0913
        self,
        kwargs: Any,
        response_obj: Any,
        start_time: Any,
        end_time: Any,
    ) -> None:
        del response_obj, start_time, end_time
        self.trace.append(
            {
                "model": str(kwargs.get("model", "unknown")),
                "sigma": 1.0,
                "verdict": "ABSTAIN",
                "error": True,
            }
        )


__all__ = [
    "SigmaLiteLLMCallback",
    "sigma_completion",
    "sigma_completion_with_fallback",
]
