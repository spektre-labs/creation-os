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


__all__ = ["sigma_completion", "sigma_completion_with_fallback"]
