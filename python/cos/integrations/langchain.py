# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""LangChain σ-gate callback: score every LLM output automatically.

Requires ``langchain-core``::

    pip install 'creation-os[langchain]'

Usage::

    from cos.integrations.langchain import SigmaGateCallback

    handler = SigmaGateCallback()
    chain.invoke({\"input\": \"...\"}, config={\"callbacks\": [handler]})

    print(handler.last_sigma, handler.last_verdict, handler.stats)
"""
from __future__ import annotations

from typing import Any, List, Optional

from cos import SigmaGate
from cos.exceptions import SigmaAbstainError

try:
    from langchain_core.callbacks import BaseCallbackHandler
    from langchain_core.outputs import LLMResult
except ImportError as e:  # pragma: no cover
    raise ImportError(
        "LangChain not installed. Run: pip install 'creation-os[langchain]'"
    ) from e


def _prompts_to_str(prompts: Any) -> List[str]:
    if prompts is None:
        return []
    if isinstance(prompts, (list, tuple)):
        return [str(p) for p in prompts]
    return [str(prompts)]


def _message_content(msg: Any) -> str:
    c = getattr(msg, "content", msg)
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


class SigmaGateCallback(BaseCallbackHandler):
    """σ-gate each LLM completion; optional raise on ABSTAIN."""

    def __init__(
        self,
        gate: Any = None,
        *,
        raise_on_abstain: bool = False,
        threshold_accept: float = 0.3,
        threshold_abstain: float = 0.8,
    ) -> None:
        super().__init__()
        self.gate = gate or SigmaGate(
            threshold_accept=threshold_accept,
            threshold_abstain=threshold_abstain,
        )
        self.raise_on_abstain = bool(raise_on_abstain)
        self.last_sigma: Optional[float] = None
        self.last_verdict: Optional[str] = None
        self.stats = {"total": 0, "accept": 0, "rethink": 0, "abstain": 0}
        self._last_prompts: List[str] = []

    def on_llm_start(
        self,
        serialized: dict[str, Any],
        prompts: list[str],
        **kwargs: Any,
    ) -> None:
        del serialized, kwargs
        self._last_prompts = _prompts_to_str(prompts)

    def on_chat_model_start(
        self,
        serialized: dict[str, Any],
        messages: list[Any],
        **kwargs: Any,
    ) -> None:
        del serialized, kwargs
        prompt = ""
        for msg_list in messages if isinstance(messages, (list, tuple)) else [messages]:
            if not isinstance(msg_list, (list, tuple)):
                msg_list = [msg_list]
            for msg in reversed(msg_list):
                role = getattr(msg, "type", None) or getattr(msg, "role", None)
                if role in ("human", "user"):
                    prompt = _message_content(msg)
                    break
                c = _message_content(msg)
                if c:
                    prompt = c
            if prompt:
                break
        self._last_prompts = [prompt] if prompt else []

    def on_llm_end(self, response: LLMResult, **kwargs: Any) -> None:
        del kwargs
        prompt = self._last_prompts[0] if self._last_prompts else ""

        for generation_list in response.generations:
            for generation in generation_list:
                text = generation.text
                if not text and hasattr(generation, "message"):
                    text = _message_content(getattr(generation, "message", ""))

                sigma, verdict = self.gate.score(prompt, str(text or ""))

                self.last_sigma = float(sigma)
                self.last_verdict = str(verdict)
                self.stats["total"] += 1
                bucket = str(verdict).lower()
                if bucket in self.stats:
                    self.stats[bucket] += 1

                meta = generation.generation_info
                merged: dict[str, Any] = dict(meta) if isinstance(meta, dict) else {}
                merged["sigma"] = float(sigma)
                merged["verdict"] = str(verdict)
                generation.generation_info = merged

                if verdict == "ABSTAIN" and self.raise_on_abstain:
                    raise SigmaAbstainError(
                        f"σ-gate ABSTAIN (σ={sigma:.3f}): response unreliable"
                    )


__all__ = ["SigmaGateCallback"]
