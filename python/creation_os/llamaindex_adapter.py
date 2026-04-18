# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2026 Spektre Labs / Lauri Rainio
"""LlamaIndex adapter for Creation OS — v142 σ-Interop.

LlamaIndex is optional.  Importing this module succeeds whether or
not ``llama_index.core`` is installed.  Use :func:`require_llamaindex`
to assert availability at call time.
"""
from __future__ import annotations

from typing import Any, List, Optional

from . import COS, ChatMessage, ChatResponse

LLAMAINDEX_AVAILABLE = False
_LLAMAINDEX_IMPORT_ERROR: Optional[str] = None

try:
    from llama_index.core.llms import LLM as _LlamaLLM  # type: ignore

    LLAMAINDEX_AVAILABLE = True
except Exception as _e:  # noqa: BLE001
    _LLAMAINDEX_IMPORT_ERROR = repr(_e)


def require_llamaindex() -> None:
    if not LLAMAINDEX_AVAILABLE:
        raise ImportError(
            "LlamaIndex is not available in this environment. "
            "Install with `pip install llama-index-core`. "
            f"(last error: {_LLAMAINDEX_IMPORT_ERROR})"
        )


class CreationOSQueryEngine:
    """Minimal, framework-optional query engine.

    When LlamaIndex is present this class delegates to the installed
    LLM base; when absent it falls back to the pure-stdlib
    :class:`creation_os.COS` client so the integration surface works
    even without LlamaIndex — matching the pattern used by the
    LangChain adapter.
    """

    def __init__(
        self,
        llm_url: str = "http://localhost:8080",
        api_key: str = "cos",
        sigma_threshold: Optional[float] = 0.5,
        model_name: str = "creation-os",
    ) -> None:
        self.cos = COS(base_url=llm_url, api_key=api_key, default_model=model_name)
        self.sigma_threshold = sigma_threshold

    def query(self, text: str, *, context: Optional[List[str]] = None) -> ChatResponse:
        messages: List[ChatMessage] = []
        if context:
            system = "Use the following context when answering:\n" + "\n---\n".join(context)
            messages.append(ChatMessage(role="system", content=system))
        messages.append(ChatMessage(role="user", content=text))
        return self.cos.chat(messages, sigma_threshold=self.sigma_threshold)
