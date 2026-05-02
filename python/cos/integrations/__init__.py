# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
"""Creation OS integrations: one-line σ hooks (optional LangChain, OpenAI, LiteLLM, …).

Concrete imports::

    from cos.integrations.langchain import SigmaGateCallback
    from cos.integrations.langchain_sigma import SigmaCallback
    from cos.integrations.decorator import sigma_gated, sigma_gated_llm, SigmaResult
    from cos.integrations.openai_wrapper import sigma_chat
    from cos.integrations.openai_sigma import SigmaOpenAI
    from cos.integrations.litellm_sigma import sigma_completion
"""
from __future__ import annotations

from typing import Any

from cos.exceptions import SigmaAbstainError

__all__ = [
    "SigmaAbstainError",
    "SigmaAutoGenHook",
    "SigmaCallback",
    "SigmaGateCallback",
    "SigmaGateHook",
    "SigmaGateLiteLLM",
    "SigmaGateTool",
    "SigmaOpenAI",
    "SigmaResult",
    "sigma_chat",
    "sigma_completion",
    "sigma_completion_with_fallback",
    "sigma_gate_node",
    "sigma_gate_router",
    "sigma_gated",
    "sigma_gated_llm",
]


def __getattr__(name: str) -> Any:
    if name == "SigmaGateCallback":
        from cos.integrations.langchain import SigmaGateCallback

        return SigmaGateCallback
    if name == "SigmaCallback":
        from cos.integrations.langchain_sigma import SigmaCallback

        return SigmaCallback
    if name == "sigma_gated":
        from cos.integrations.decorator import sigma_gated

        return sigma_gated
    if name == "sigma_gated_llm":
        from cos.integrations.decorator import sigma_gated_llm

        return sigma_gated_llm
    if name == "SigmaResult":
        from cos.integrations.decorator import SigmaResult

        return SigmaResult
    if name == "sigma_chat":
        from cos.integrations.openai_wrapper import sigma_chat

        return sigma_chat
    if name == "SigmaGateLiteLLM":
        from cos.integrations.litellm import SigmaGateLiteLLM

        return SigmaGateLiteLLM
    if name == "SigmaGateTool":
        from cos.integrations.crewai_sigma import SigmaGateTool

        return SigmaGateTool
    if name == "SigmaAutoGenHook":
        from cos.integrations.autogen_sigma import SigmaAutoGenHook

        return SigmaAutoGenHook
    if name == "SigmaGateHook":
        from cos.integrations.autogen_sigma import SigmaGateHook

        return SigmaGateHook
    if name == "SigmaOpenAI":
        from cos.integrations.openai_sigma import SigmaOpenAI

        return SigmaOpenAI
    if name == "sigma_completion":
        from cos.integrations.litellm_sigma import sigma_completion

        return sigma_completion
    if name == "sigma_completion_with_fallback":
        from cos.integrations.litellm_sigma import sigma_completion_with_fallback

        return sigma_completion_with_fallback
    if name in ("sigma_gate_node", "sigma_gate_router"):
        from cos.integrations import langgraph_sigma

        return getattr(langgraph_sigma, name)
    raise AttributeError(f"module {__name__!r} has no attribute {name!r}")
