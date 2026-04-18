# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2026 Spektre Labs / Lauri Rainio
"""LangChain adapter for Creation OS — v142 σ-Interop.

LangChain is an **optional** dependency.  Importing this module
succeeds whether or not ``langchain_core`` is installed.  The
:class:`CreationOSLLM` class is defined only when LangChain is
available; otherwise :data:`LANGCHAIN_AVAILABLE` is False and
:func:`require_langchain()` raises a helpful ImportError.
"""
from __future__ import annotations

from typing import Any, Dict, List, Optional

from . import COS, ChatResponse

LANGCHAIN_AVAILABLE = False
_LANGCHAIN_IMPORT_ERROR: Optional[str] = None

try:
    from langchain_core.language_models.chat_models import BaseChatModel  # type: ignore
    from langchain_core.messages import AIMessage, BaseMessage, HumanMessage, SystemMessage  # type: ignore
    from langchain_core.outputs import ChatGeneration, ChatResult  # type: ignore

    LANGCHAIN_AVAILABLE = True
except Exception as _e:  # noqa: BLE001
    _LANGCHAIN_IMPORT_ERROR = repr(_e)


def require_langchain() -> None:
    if not LANGCHAIN_AVAILABLE:
        raise ImportError(
            "LangChain is not available in this environment. "
            "Install with `pip install langchain-core`. "
            f"(last error: {_LANGCHAIN_IMPORT_ERROR})"
        )


if LANGCHAIN_AVAILABLE:

    class CreationOSLLM(BaseChatModel):  # type: ignore[misc]
        """LangChain :class:`BaseChatModel` backed by Creation OS v106."""

        base_url: str = "http://localhost:8080"
        api_key: str = "cos"
        sigma_threshold: Optional[float] = None
        model_name: str = "creation-os"

        @property
        def _llm_type(self) -> str:
            return "creation-os"

        def _generate(
            self,
            messages: List[BaseMessage],
            stop: Optional[List[str]] = None,
            run_manager: Any = None,
            **kwargs: Any,
        ) -> ChatResult:
            cos = COS(
                base_url=self.base_url,
                api_key=self.api_key,
                default_model=self.model_name,
            )
            cos_messages: List[Dict[str, str]] = []
            for m in messages:
                if isinstance(m, SystemMessage):
                    role = "system"
                elif isinstance(m, AIMessage):
                    role = "assistant"
                else:
                    role = "user"
                cos_messages.append({"role": role, "content": str(m.content)})

            resp: ChatResponse = cos.chat(
                cos_messages,
                sigma_threshold=kwargs.get("sigma_threshold", self.sigma_threshold),
                temperature=kwargs.get("temperature"),
                max_tokens=kwargs.get("max_tokens"),
            )
            ai = AIMessage(
                content=resp.text,
                additional_kwargs={
                    "sigma_product": resp.sigma_product,
                    "specialist":    resp.specialist,
                    "x_cos_headers": resp.headers,
                },
            )
            return ChatResult(generations=[ChatGeneration(message=ai)])
