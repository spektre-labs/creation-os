# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2026 Spektre Labs / Lauri Rainio
"""Creation OS Python SDK — v142 σ-Interop.

Single-file, stdlib-only Python client for the v106 HTTP kernel.
Every response carries ``sigma_product`` and ``specialist`` metadata
extracted from the ``X-COS-*`` headers (v106 already emits them), so
downstream frameworks (LangChain, LlamaIndex, AutoGen) can gate on
σ without reading the transport layer directly.

v142.0 ships:

* ``creation_os.COS`` — thin chat client with σ-annotated responses.
* ``creation_os.ChatResponse`` — a named tuple with .text,
  .sigma_product, .specialist, .raw, and .role.
* ``creation_os.langchain_adapter.CreationOSLLM`` — lazy-imports
  langchain, degrades gracefully if unavailable.
* ``creation_os.llamaindex_adapter.CreationOSLLM`` — same pattern.
* ``creation_os.openai_compat`` — proves the v106 HTTP shape matches
  the OpenAI Chat Completions shape (tested by the merge-gate with
  a fixture, no network).

The SDK has **no hard external dependencies** — it works with just the
Python standard library.  Framework integrations are all optional.
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Dict, Iterable, List, Optional
import json
import urllib.error
import urllib.request

__all__ = [
    "COS",
    "ChatResponse",
    "ChatMessage",
    "COSError",
]

__version__ = "0.1.0"


class COSError(RuntimeError):
    """Raised when the Creation OS HTTP kernel returns an error."""


@dataclass(frozen=True)
class ChatMessage:
    role: str
    content: str

    def to_dict(self) -> Dict[str, str]:
        return {"role": self.role, "content": self.content}


@dataclass(frozen=True)
class ChatResponse:
    text: str
    role: str
    sigma_product: Optional[float]
    specialist: Optional[str]
    headers: Dict[str, str]
    raw: Dict[str, Any]

    @property
    def emitted(self) -> bool:
        """True if the response was emitted (not abstained)."""
        return self.text != "" and self.raw.get("finish_reason") != "abstain"


class COS:
    """OpenAI-compatible chat client for Creation OS v106.

    Example:
        cos = COS(base_url="http://localhost:8080")
        r = cos.chat("Hello", sigma_threshold=0.5)
        print(r.text, r.sigma_product, r.specialist)
    """

    def __init__(
        self,
        base_url: str = "http://localhost:8080",
        api_key: str = "cos",
        timeout: float = 30.0,
        default_model: str = "creation-os",
    ) -> None:
        self.base_url = base_url.rstrip("/")
        self.api_key = api_key
        self.timeout = timeout
        self.default_model = default_model

    def chat(
        self,
        prompt: str | Iterable[ChatMessage] | List[Dict[str, str]],
        *,
        model: Optional[str] = None,
        sigma_threshold: Optional[float] = None,
        temperature: Optional[float] = None,
        max_tokens: Optional[int] = None,
        extra_headers: Optional[Dict[str, str]] = None,
    ) -> ChatResponse:
        messages = self._normalize_messages(prompt)
        body: Dict[str, Any] = {
            "model": model or self.default_model,
            "messages": messages,
        }
        if sigma_threshold is not None:
            # v106 reads both JSON body *and* X-COS-Sigma-Threshold.
            body["sigma_threshold"] = sigma_threshold
        if temperature is not None:
            body["temperature"] = temperature
        if max_tokens is not None:
            body["max_tokens"] = max_tokens

        headers = {
            "Content-Type": "application/json",
            "Authorization": f"Bearer {self.api_key}",
        }
        if sigma_threshold is not None:
            headers["X-COS-Sigma-Threshold"] = f"{sigma_threshold:.6f}"
        if extra_headers:
            headers.update(extra_headers)

        req = urllib.request.Request(
            url=f"{self.base_url}/v1/chat/completions",
            data=json.dumps(body).encode("utf-8"),
            headers=headers,
            method="POST",
        )
        try:
            with urllib.request.urlopen(req, timeout=self.timeout) as resp:
                raw = json.loads(resp.read().decode("utf-8"))
                resp_headers = {k.lower(): v for k, v in resp.headers.items()}
        except urllib.error.HTTPError as e:
            raise COSError(f"HTTP {e.code}: {e.reason}") from e
        except urllib.error.URLError as e:
            raise COSError(f"transport error: {e.reason}") from e

        return self._parse(raw, resp_headers)

    # ------------------------------------------------------------------
    # Internals
    # ------------------------------------------------------------------
    def _normalize_messages(self, prompt: Any) -> List[Dict[str, str]]:
        if isinstance(prompt, str):
            return [{"role": "user", "content": prompt}]
        out: List[Dict[str, str]] = []
        for m in prompt:
            if isinstance(m, ChatMessage):
                out.append(m.to_dict())
            elif isinstance(m, dict) and "role" in m and "content" in m:
                out.append({"role": str(m["role"]), "content": str(m["content"])})
            else:
                raise TypeError(
                    "messages must be str, ChatMessage, or dict "
                    "with role/content"
                )
        return out

    def _parse(self, raw: Dict[str, Any], headers: Dict[str, str]) -> ChatResponse:
        # OpenAI-compatible shape: raw["choices"][0]["message"]["content"].
        try:
            choice = raw["choices"][0]
            text = choice["message"]["content"]
            role = choice["message"].get("role", "assistant")
        except (KeyError, IndexError, TypeError) as e:
            raise COSError(f"malformed chat response: {e}; body={raw!r}") from e

        sigma = None
        if "x-cos-sigma-product" in headers:
            try:
                sigma = float(headers["x-cos-sigma-product"])
            except (TypeError, ValueError):
                sigma = None
        # Fall back: some deployments echo sigma in the JSON body.
        if sigma is None and isinstance(raw.get("x_cos_sigma_product"), (int, float)):
            sigma = float(raw["x_cos_sigma_product"])

        specialist = headers.get("x-cos-specialist")
        if specialist is None and isinstance(raw.get("x_cos_specialist"), str):
            specialist = raw["x_cos_specialist"]

        return ChatResponse(
            text=text,
            role=role,
            sigma_product=sigma,
            specialist=specialist,
            headers=headers,
            raw=raw,
        )

    # ------------------------------------------------------------------
    # Parse-only path used by the merge-gate (no network).
    # ------------------------------------------------------------------
    def parse_openai_response(
        self,
        raw: Dict[str, Any],
        headers: Optional[Dict[str, str]] = None,
    ) -> ChatResponse:
        """Parse a raw OpenAI-compat response dict + X-COS-* headers.

        Used by :mod:`creation_os.openai_compat` smoke tests without
        requiring a live v106 endpoint.
        """
        return self._parse(raw, {(k.lower()): v for k, v in (headers or {}).items()})
