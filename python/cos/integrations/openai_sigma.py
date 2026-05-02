# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""OpenAI SDK convenience wrapper: one call → completion text + σ + verdict.

Requires the OpenAI Python package when methods are used::

    pip install 'creation-os[openai]'
"""
from __future__ import annotations

from typing import Any, Dict, Iterator, List, MutableMapping, Optional, Sequence

from cos import SigmaGate
from cos.integrations.openai_wrapper import _choice_text, _last_user_content


class SigmaOpenAI:
    """Lazily construct ``openai.OpenAI`` and run gated chat helpers."""

    def __init__(
        self,
        gate: Any = None,
        *,
        api_key: Optional[str] = None,
        base_url: Optional[str] = None,
    ) -> None:
        self.gate = gate or SigmaGate()
        self.api_key = api_key
        self.base_url = base_url
        self._client: Any = None

    def _get_client(self) -> Any:
        if self._client is None:
            try:
                import openai
            except ImportError as e:
                raise ImportError("pip install openai") from e
            kw: Dict[str, Any] = {}
            if self.api_key:
                kw["api_key"] = self.api_key
            if self.base_url:
                kw["base_url"] = self.base_url
            self._client = openai.OpenAI(**kw)
        return self._client

    def chat(
        self,
        prompt: str,
        model: str = "gpt-4o",
        *,
        temperature: float = 0.0,
        **kwargs: Any,
    ) -> Dict[str, Any]:
        client = self._get_client()
        messages: Sequence[MutableMapping[str, Any]] = kwargs.pop(
            "messages",
            None,
        ) or [{"role": "user", "content": prompt}]
        response = client.chat.completions.create(
            model=model,
            messages=list(messages),
            temperature=temperature,
            **kwargs,
        )
        text = _choice_text(response)
        user_msg = _last_user_content(messages) or prompt
        sigma, verdict = self.gate.score(user_msg, text)
        usage = getattr(response, "usage", None)
        usage_out = None
        if usage is not None:
            usage_out = {
                "prompt_tokens": getattr(usage, "prompt_tokens", None),
                "completion_tokens": getattr(usage, "completion_tokens", None),
            }
        return {
            "response": text,
            "sigma": float(sigma),
            "verdict": str(verdict),
            "model": model,
            "usage": usage_out,
            "raw": response,
        }

    def chat_stream(
        self,
        prompt: str,
        model: str = "gpt-4o",
        **kwargs: Any,
    ) -> Iterator[Dict[str, Any]]:
        client = self._get_client()
        messages: List[Dict[str, Any]] = list(
            kwargs.pop("messages", None) or [{"role": "user", "content": prompt}]
        )
        stream = client.chat.completions.create(
            model=model,
            messages=messages,
            stream=True,
            **kwargs,
        )
        chunks: List[str] = []
        for chunk in stream:
            try:
                delta = chunk.choices[0].delta
                piece = getattr(delta, "content", None) or ""
            except (AttributeError, IndexError, TypeError):
                piece = ""
            if piece:
                chunks.append(str(piece))
                yield {"token": str(piece), "text_so_far": "".join(chunks)}
        full_text = "".join(chunks)
        user_msg = _last_user_content(messages) or prompt
        sigma, verdict = self.gate.score(user_msg, full_text)
        yield {
            "token": None,
            "text_so_far": full_text,
            "sigma": float(sigma),
            "verdict": str(verdict),
            "is_final": True,
        }


__all__ = ["SigmaOpenAI"]
