# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
# Source:        https://github.com/spektre-labs/creation-os-kernel
# Website:       https://spektrelabs.org
# Commercial:    spektre.labs@proton.me
# License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
"""σ-gated multi-turn chat via an OpenAI-compatible HTTP API (vLLM, SGLang, llama.cpp, …).

Requires ``openai`` (optional extra): ``pip install 'creation-os[openai]'``.

Default local stack targets **Qwen3.6**-class models with optional
``preserve_thinking`` (forwarded only when the model id looks like Qwen).
"""
from __future__ import annotations

import json
import os
import sys
from typing import Any, Callable, Dict, List, Optional, Tuple

from cos.sigma_gate import ABSTAIN, ACCEPT, RETHINK

DEFAULT_CHAT_ENDPOINT = "http://localhost:8000/v1"
DEFAULT_CHAT_MODEL = "Qwen/Qwen3.6-35B-A3B"

OPENAI_INSTALL_HINT = "Install the OpenAI client: pip install 'creation-os[openai]'"


def _env_endpoint() -> str:
    return (os.environ.get("CREATION_OS_CHAT_ENDPOINT") or DEFAULT_CHAT_ENDPOINT).rstrip("/")


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
    """Extract assistant text from a raw OpenAI-style ``chat.completion`` JSON object."""
    choices = data.get("choices") or []
    if not choices:
        return ""
    msg = (choices[0] or {}).get("message") or {}
    return _message_content_to_text(msg.get("content"))


def assistant_text_from_response(resp: Any) -> str:
    """Normalize OpenAI SDK completion object to assistant string."""
    if not getattr(resp, "choices", None):
        return ""
    ch0 = resp.choices[0]
    msg = getattr(ch0, "message", None)
    if msg is None:
        return ""
    return _message_content_to_text(getattr(msg, "content", None))


def should_preserve_thinking(preserve_thinking: bool, model: str) -> bool:
    """Qwen chat templates honor ``preserve_thinking``; other stacks typically ignore or error."""
    if not preserve_thinking:
        return False
    m = model.lower()
    return "qwen" in m


def build_completion_kwargs(
    *,
    model: str,
    messages: List[Dict[str, Any]],
    preserve_thinking: bool,
) -> Dict[str, Any]:
    kwargs: Dict[str, Any] = {"model": model, "messages": messages}
    if should_preserve_thinking(preserve_thinking, model):
        kwargs["extra_body"] = {"chat_template_kwargs": {"preserve_thinking": True}}
    return kwargs


def openai_client_factory(
    *,
    base_url: str,
    api_key: str,
) -> Any:
    try:
        from openai import OpenAI
    except ImportError as e:  # pragma: no cover - exercised via tests that patch
        raise ImportError(OPENAI_INSTALL_HINT) from e
    return OpenAI(base_url=base_url.rstrip("/"), api_key=api_key)


def complete_chat(
    client: Any,
    *,
    model: str,
    messages: List[Dict[str, Any]],
    preserve_thinking: bool,
) -> Any:
    kwargs = build_completion_kwargs(model=model, messages=messages, preserve_thinking=preserve_thinking)
    return client.chat.completions.create(**kwargs)


def score_reply(gate: Any, user_prompt: str, assistant: str) -> Tuple[float, str]:
    return gate.score(user_prompt, assistant)


def format_user_output(
    *,
    verdict: str,
    sigma: float,
    text: str,
    json_mode: bool,
) -> Tuple[str, int]:
    """Returns (line_or_json, exit_code). Exit 2 on ABSTAIN to match other cos commands."""
    if json_mode:
        out = {
            "verdict": verdict,
            "sigma": round(float(sigma), 6),
            "text": text,
        }
        return json.dumps(out, ensure_ascii=False), (2 if verdict == ABSTAIN else 0)

    if verdict == ACCEPT:
        return text, 0
    if verdict == RETHINK:
        return f"{text}\n\nσ={sigma:.3f} — verify this", 0
    if verdict == ABSTAIN:
        return f"I don't know (σ={sigma:.3f})", 2
    return f"{text}\n\n[σ={sigma:.3f} {verdict}]", 0


def run_turn(
    *,
    gate: Any,
    client: Any,
    model: str,
    messages: List[Dict[str, Any]],
    preserve_thinking: bool,
) -> Tuple[List[Dict[str, Any]], float, str, str]:
    """Perform one model call + σ score; append assistant message to *messages* in place."""
    resp = complete_chat(client, model=model, messages=messages, preserve_thinking=preserve_thinking)
    assistant = assistant_text_from_response(resp)
    user_prompt = last_user_text(messages)
    sigma, verdict = score_reply(gate, user_prompt, assistant)
    messages.append({"role": "assistant", "content": assistant})
    return messages, float(sigma), str(verdict), assistant


def chat_repl(
    *,
    gate: Any,
    client: Any,
    model: str,
    preserve_thinking: bool,
    json_mode: bool,
    verbose: bool,
    print_fn: Callable[..., None],
    input_fn: Callable[[str], str],
) -> int:
    messages: List[Dict[str, Any]] = []
    exit_code = 0
    print_fn("Creation OS σ-chat (empty line or 'quit' to exit)")
    while True:
        try:
            user = input_fn("> ").strip()
        except EOFError:
            break
        if not user or user.lower() in {"quit", "exit"}:
            break
        messages.append({"role": "user", "content": user})
        try:
            _, sigma, verdict, assistant = run_turn(
                gate=gate,
                client=client,
                model=model,
                messages=messages,
                preserve_thinking=preserve_thinking,
            )
        except Exception as e:
            # pop user message so history stays consistent
            messages.pop()
            print_fn(f"error: chat request failed: {e}", file=sys.stderr)
            return 1
        line, code = format_user_output(verdict=verdict, sigma=sigma, text=assistant, json_mode=json_mode)
        print_fn(line)
        if verbose and not json_mode:
            print_fn(f"[σ={sigma:.4f} {verdict}]", file=sys.stderr)
        exit_code = max(exit_code, code)
    return exit_code


def chat_single_turn(
    *,
    gate: Any,
    client: Any,
    model: str,
    prompt: str,
    preserve_thinking: bool,
    json_mode: bool,
    verbose: bool,
    print_fn: Callable[..., None],
) -> int:
    messages: List[Dict[str, Any]] = [{"role": "user", "content": prompt}]
    try:
        _, sigma, verdict, assistant = run_turn(
            gate=gate,
            client=client,
            model=model,
            messages=messages,
            preserve_thinking=preserve_thinking,
        )
    except Exception as e:
        print_fn(
            f"error: no response from chat endpoint (check server and CREATION_OS_CHAT_ENDPOINT): {e}",
            file=sys.stderr,
        )
        return 1
    line, code = format_user_output(verdict=verdict, sigma=sigma, text=assistant, json_mode=json_mode)
    print_fn(line)
    if verbose and not json_mode:
        print_fn(f"[σ={sigma:.4f} {verdict}]", file=sys.stderr)
    return code


def run_from_cli_args(args: Any) -> int:
    """Entry point for ``cos chat`` (``argparse.Namespace``)."""
    from cos import SigmaGate

    preserve_thinking = not bool(getattr(args, "chat_no_think", False))
    model = str(getattr(args, "chat_model", DEFAULT_CHAT_MODEL) or DEFAULT_CHAT_MODEL)
    endpoint = str(getattr(args, "chat_endpoint", "") or "").strip() or _env_endpoint()
    api_key = str(getattr(args, "chat_api_key", "") or "").strip() or (
        os.environ.get("OPENAI_API_KEY") or "local"
    )
    json_mode = bool(getattr(args, "out_json", False))
    verbose = bool(getattr(args, "cli_verbose", False))
    prompt = str(getattr(args, "chat_prompt", "") or "").strip()

    try:
        client = openai_client_factory(base_url=endpoint, api_key=api_key)
    except ImportError as e:
        print(str(e), file=sys.stderr)
        return 1

    gate = SigmaGate()

    if prompt:
        return chat_single_turn(
            gate=gate,
            client=client,
            model=model,
            prompt=prompt,
            preserve_thinking=preserve_thinking,
            json_mode=json_mode,
            verbose=verbose,
            print_fn=print,
        )

    return chat_repl(
        gate=gate,
        client=client,
        model=model,
        preserve_thinking=preserve_thinking,
        json_mode=json_mode,
        verbose=verbose,
        print_fn=print,
        input_fn=input,
    )


__all__ = [
    "DEFAULT_CHAT_ENDPOINT",
    "DEFAULT_CHAT_MODEL",
    "OPENAI_INSTALL_HINT",
    "assistant_text_from_completion_dict",
    "assistant_text_from_response",
    "build_completion_kwargs",
    "chat_repl",
    "chat_single_turn",
    "complete_chat",
    "format_user_output",
    "last_user_text",
    "openai_client_factory",
    "run_from_cli_args",
    "run_turn",
    "score_reply",
    "should_preserve_thinking",
]
