# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
# Source:        https://github.com/spektre-labs/creation-os-kernel
# Website:       https://spektrelabs.org
# Commercial:    spektre.labs@proton.me
# License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
"""σ-gated chat — any OpenAI-compatible backend (vLLM, SGLang, llama.cpp, LM Studio, Ollama, …).

Requires optional ``openai``: ``pip install 'creation-os[openai]'`` or ``pip install 'creation-os[chat]'``.

Environment (optional): ``COS_ENDPOINT``, ``COS_MODEL``, ``COS_API_KEY`` — also
``CREATION_OS_CHAT_ENDPOINT`` / ``OPENAI_API_KEY`` for compatibility.
"""
from __future__ import annotations

import json
import os
import sys
import uuid
from typing import Any, Dict, Iterator, List, Optional, Tuple

try:
    from openai import OpenAI

    _HAS_OPENAI = True
except ImportError:  # pragma: no cover - alternate path in SigmaChat.__init__
    OpenAI = None  # type: ignore[misc, assignment]
    _HAS_OPENAI = False

from cos.sigma_gate import ABSTAIN, ACCEPT, RETHINK, SigmaGate

DEFAULT_CHAT_ENDPOINT = "http://localhost:8000/v1"
DEFAULT_CHAT_MODEL = "Qwen/Qwen3.6-35B-A3B"

OPENAI_INSTALL_HINT = "openai not installed. Run: pip install 'creation-os[openai]'"


def _resolved_endpoint(explicit: Optional[str]) -> str:
    return (
        (explicit or "").strip()
        or os.environ.get("COS_ENDPOINT", "").strip()
        or os.environ.get("CREATION_OS_CHAT_ENDPOINT", "").strip()
        or DEFAULT_CHAT_ENDPOINT
    ).rstrip("/")


def _resolved_model(explicit: Optional[str]) -> str:
    return (explicit or "").strip() or os.environ.get("COS_MODEL", "").strip() or DEFAULT_CHAT_MODEL


def _resolved_api_key(explicit: Optional[str]) -> str:
    return (
        (explicit or "").strip()
        or os.environ.get("COS_API_KEY", "").strip()
        or os.environ.get("OPENAI_API_KEY", "").strip()
        or "not-needed"
    )


def _message_content_to_text(content: Any) -> str:
    if content is None:
        return ""
    if isinstance(content, str):
        return content
    if isinstance(content, list):
        parts: List[str] = []
        for block in content:
            if isinstance(block, dict):
                t = block.get("text") or block.get("content")
                parts.append(str(t) if t is not None else "")
            else:
                parts.append(str(block))
        return "".join(parts)
    return str(content)


def last_user_text(messages: List[Dict[str, Any]]) -> str:
    """Plain text of the latest ``role=user`` message (for σ scoring)."""
    for m in reversed(messages):
        if str(m.get("role", "")).lower() == "user":
            return _message_content_to_text(m.get("content")).strip()
    return ""


def assistant_text_from_completion_dict(data: Dict[str, Any]) -> str:
    choices = data.get("choices") or []
    if not choices:
        return ""
    msg = (choices[0] or {}).get("message") or {}
    return _message_content_to_text(msg.get("content"))


def assistant_text_from_response(resp: Any) -> str:
    if not getattr(resp, "choices", None):
        return ""
    ch0 = resp.choices[0]
    msg = getattr(ch0, "message", None)
    if msg is None:
        return ""
    return _message_content_to_text(getattr(msg, "content", None))


def should_preserve_thinking(preserve_thinking: bool, model: str) -> bool:
    """Only Qwen-class chat templates reliably accept ``preserve_thinking``."""
    if not preserve_thinking:
        return False
    return "qwen" in model.lower()


def _extra_body_for_model(preserve_thinking: bool, model: str) -> Optional[Dict[str, Any]]:
    if not should_preserve_thinking(preserve_thinking, model):
        return None
    return {"chat_template_kwargs": {"preserve_thinking": True}}


class SigmaChat:
    """Multi-turn σ-gated chat against one OpenAI-compatible HTTP server."""

    def __init__(
        self,
        endpoint: Optional[str] = None,
        model: Optional[str] = None,
        api_key: Optional[str] = None,
        gate: Optional[SigmaGate] = None,
        preserve_thinking: bool = True,
    ) -> None:
        if not _HAS_OPENAI or OpenAI is None:
            raise ImportError(OPENAI_INSTALL_HINT)
        self.endpoint = _resolved_endpoint(endpoint)
        self.model = _resolved_model(model)
        self.api_key = _resolved_api_key(api_key)
        self.client = OpenAI(base_url=self.endpoint, api_key=self.api_key)
        self.gate = gate or SigmaGate()
        self.preserve_thinking = preserve_thinking
        self.messages: List[Dict[str, Any]] = []
        self.history: List[Tuple[str, str, float, str]] = []

    def reset(self) -> None:
        self.messages.clear()
        self.history.clear()

    def session_sigma(self) -> float:
        if not self.history:
            return 0.0
        return sum(h[2] for h in self.history) / len(self.history)

    def send(
        self,
        user_message: str,
        system: Optional[str] = None,
        *,
        temperature: float = 0.7,
        max_tokens: int = 32768,
    ) -> Dict[str, Any]:
        if system and not self.messages:
            self.messages.append({"role": "system", "content": system})
        self.messages.append({"role": "user", "content": user_message})

        eb = _extra_body_for_model(self.preserve_thinking, self.model)
        try:
            req: Dict[str, Any] = {
                "model": self.model,
                "messages": [dict(m) for m in self.messages],
                "temperature": temperature,
                "max_tokens": max_tokens,
            }
            if eb is not None:
                req["extra_body"] = eb
            resp = self.client.chat.completions.create(**req)
        except Exception as e:
            self.messages.pop()
            return {
                "text": None,
                "sigma": 1.0,
                "verdict": ABSTAIN,
                "error": str(e),
            }

        text = assistant_text_from_response(resp) or ""
        sigma, verdict = self.gate.score(user_message, text)
        self.messages.append({"role": "assistant", "content": text})
        self.history.append((user_message, text, float(sigma), str(verdict)))
        return {"text": text, "sigma": float(sigma), "verdict": str(verdict), "error": None}

    def send_stream(
        self,
        user_message: str,
        system: Optional[str] = None,
        *,
        temperature: float = 0.7,
        max_tokens: int = 32768,
    ) -> Iterator[Dict[str, Any]]:
        if system and not self.messages:
            self.messages.append({"role": "system", "content": system})
        self.messages.append({"role": "user", "content": user_message})

        eb = _extra_body_for_model(self.preserve_thinking, self.model)
        try:
            req: Dict[str, Any] = {
                "model": self.model,
                "messages": [dict(m) for m in self.messages],
                "temperature": temperature,
                "max_tokens": max_tokens,
                "stream": True,
            }
            if eb is not None:
                req["extra_body"] = eb
            stream = self.client.chat.completions.create(**req)
        except Exception as e:
            self.messages.pop()
            yield {
                "chunk": None,
                "done": True,
                "sigma": 1.0,
                "verdict": ABSTAIN,
                "error": str(e),
            }
            return

        full_text: List[str] = []
        for chunk in stream:
            delta = ""
            try:
                if chunk.choices and chunk.choices[0].delta is not None:
                    delta = chunk.choices[0].delta.content or ""
            except Exception:
                delta = ""
            full_text.append(delta)
            yield {"chunk": delta, "done": False, "sigma": None, "verdict": None, "error": None}

        text = "".join(full_text)
        sigma, verdict = self.gate.score(user_message, text)
        self.messages.append({"role": "assistant", "content": text})
        self.history.append((user_message, text, float(sigma), str(verdict)))
        yield {"chunk": None, "done": True, "sigma": float(sigma), "verdict": str(verdict), "error": None}

    def complete_from_openai_request(self, body: Dict[str, Any]) -> Dict[str, Any]:
        """Run one non-streaming completion from a raw OpenAI ``chat.completions`` body."""
        messages = [dict(m) for m in (body.get("messages") or [])]
        if not messages:
            return {"error": "messages required", "text": None, "sigma": 1.0, "verdict": ABSTAIN}
        if body.get("stream"):
            return {"error": "stream=true not supported in SigmaChat.complete_from_openai_request", "text": None, "sigma": 1.0, "verdict": ABSTAIN}

        model = str(body.get("model") or self.model)
        temperature = float(body.get("temperature", 0.7))
        max_tokens = int(body.get("max_tokens", 32768))

        eb = _extra_body_for_model(self.preserve_thinking, model)
        try:
            req: Dict[str, Any] = {
                "model": model,
                "messages": messages,
                "temperature": temperature,
                "max_tokens": max_tokens,
            }
            if eb is not None:
                req["extra_body"] = eb
            resp = self.client.chat.completions.create(**req)
        except Exception as e:
            return {"text": None, "sigma": 1.0, "verdict": ABSTAIN, "error": str(e)}

        text = assistant_text_from_response(resp) or ""
        user_prompt = last_user_text(messages)
        sigma, verdict = self.gate.score(user_prompt, text)
        out_id = f"chatcmpl-cos-{uuid.uuid4().hex[:12]}"
        return {
            "id": out_id,
            "object": "chat.completion",
            "model": model,
            "choices": [
                {
                    "index": 0,
                    "message": {"role": "assistant", "content": text},
                    "finish_reason": "stop",
                }
            ],
            "sigma": float(sigma),
            "verdict": str(verdict),
            "creation_os": {"sigma": round(float(sigma), 6), "verdict": str(verdict)},
            "error": None,
        }


def format_user_output(
    *,
    verdict: str,
    sigma: float,
    text: str,
    json_mode: bool,
) -> Tuple[str, int]:
    if json_mode:
        out = {"verdict": verdict, "sigma": round(float(sigma), 6), "text": text}
        return json.dumps(out, ensure_ascii=False), (2 if verdict == ABSTAIN else 0)
    if verdict == ACCEPT:
        return text, 0
    if verdict == RETHINK:
        return f"{text}\n\nσ={sigma:.3f} — verify this", 0
    if verdict == ABSTAIN:
        return f"I don't know (σ={sigma:.3f})", 2
    return f"{text}\n\n[σ={sigma:.3f} {verdict}]", 0


def run_from_cli_args(args: Any) -> int:
    """Backward-compatible entry for ``argparse.Namespace`` (``cos chat``)."""
    preserve_thinking = not bool(getattr(args, "chat_no_think", False))
    model = str(getattr(args, "chat_model", "") or "") or None
    endpoint = str(getattr(args, "chat_endpoint", "") or "") or None
    api_key = str(getattr(args, "chat_api_key", "") or "") or None
    json_mode = bool(getattr(args, "out_json", False))
    verbose = bool(getattr(args, "cli_verbose", False))
    prompt = str(getattr(args, "chat_prompt", "") or "").strip()
    system = str(getattr(args, "chat_system", "") or "").strip() or None

    try:
        chat = SigmaChat(
            endpoint=endpoint,
            model=model,
            api_key=api_key,
            preserve_thinking=preserve_thinking,
        )
    except ImportError as e:
        print(str(e), file=sys.stderr)
        return 1

    if prompt:
        result = chat.send(prompt, system=system)
        if result["error"]:
            print(f"error: {result['error']}", file=sys.stderr)
            return 1
        line, code = format_user_output(
            verdict=str(result["verdict"]),
            sigma=float(result["sigma"]),
            text=str(result["text"] or ""),
            json_mode=json_mode,
        )
        print(line)
        if verbose and not json_mode:
            print(f"[σ={float(result['sigma']):.4f} {result['verdict']}]", file=sys.stderr)
        return code

    print("Creation OS σ-chat (type 'quit' to exit)")
    if system:
        print(f"System: {system}")
    exit_code = 0
    while True:
        try:
            user = input("\n> ").strip()
        except (EOFError, KeyboardInterrupt):
            break
        if not user or user.lower() in ("quit", "exit", "q"):
            break
        result = chat.send(user, system=system)
        if result["error"]:
            print(f"Error: {result['error']}")
            exit_code = 1
            continue
        if json_mode:
            print(json.dumps(result, default=str))
        else:
            print(f"\n[σ={result['sigma']:.3f} {result['verdict']}]")
            print(result["text"])
        if result["verdict"] == ABSTAIN:
            exit_code = max(exit_code, 2)

    if chat.history:
        print(f"\nSession σ avg: {chat.session_sigma():.3f} over {len(chat.history)} turns")
    return exit_code


__all__ = [
    "DEFAULT_CHAT_ENDPOINT",
    "DEFAULT_CHAT_MODEL",
    "OPENAI_INSTALL_HINT",
    "SigmaChat",
    "assistant_text_from_completion_dict",
    "assistant_text_from_response",
    "format_user_output",
    "last_user_text",
    "run_from_cli_args",
    "should_preserve_thinking",
]
