# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""``@sigma_gated`` — wrap a callable; return :class:`SigmaResult` or a metrics dict."""
from __future__ import annotations

import functools
import time
from typing import Any, Callable, Dict, Optional, TypeVar, Union, overload

from cos import SigmaGate
from cos.exceptions import SigmaAbstainError

F = TypeVar("F", bound=Callable[..., Any])


class SigmaResult:
    """Text response plus σ metadata (truthy as string → ``text``)."""

    __slots__ = ("text", "sigma", "verdict")

    def __init__(self, text: str, sigma: float, verdict: str) -> None:
        self.text = text
        self.sigma = float(sigma)
        self.verdict = str(verdict)

    def __str__(self) -> str:
        return self.text

    def __repr__(self) -> str:
        clip = self.text[:50] + ("…" if len(self.text) > 50 else "")
        return f"SigmaResult(σ={self.sigma:.3f}, {self.verdict!r}, {clip!r})"


def _coerce_response(result: Any, args: tuple[Any, ...], kwargs: Dict[str, Any]) -> tuple[str, Any]:
    if isinstance(result, str):
        return result, result
    if isinstance(result, dict) and "content" in result:
        return str(result["content"]), result
    if isinstance(result, dict) and "text" in result:
        return str(result["text"]), result
    return str(result), result


@overload
def sigma_gated(func: F) -> F: ...


@overload
def sigma_gated(
    func: None = None,
    *,
    gate: Optional[Any] = None,
    return_dict: bool = ...,
    on_abstain: str = ...,
    threshold_accept: float = ...,
    threshold_abstain: float = ...,
    raise_on_abstain: bool = ...,
) -> Callable[[F], F]: ...


def sigma_gated(
    func: Optional[F] = None,
    *,
    gate: Optional[Any] = None,
    return_dict: bool = False,
    on_abstain: str = "return",
    threshold_accept: float = 0.3,
    threshold_abstain: float = 0.8,
    raise_on_abstain: bool = False,
) -> Union[F, Callable[[F], F]]:
    """Decorator: first positional arg = prompt; score string-like return value."""

    def decorator(fn: F) -> F:
        g = gate or SigmaGate(threshold_accept=threshold_accept, threshold_abstain=threshold_abstain)

        @functools.wraps(fn)
        def wrapper(*args: Any, **kwargs: Any) -> Union[SigmaResult, Dict[str, Any]]:
            if return_dict:
                t0 = time.monotonic()
                result = fn(*args, **kwargs)
                prompt = str(args[0]) if args else str(kwargs.get("prompt", "") or "")
                response_text, payload = _coerce_response(result, args, kwargs)
                sigma, verdict = g.score(prompt, response_text)
                elapsed_ms = (time.monotonic() - t0) * 1000.0
                if verdict == "ABSTAIN" and on_abstain == "raise":
                    raise SigmaAbstainError(f"σ-gate ABSTAIN: σ={sigma:.3f}")
                if verdict == "ABSTAIN" and on_abstain == "retry":
                    result = fn(*args, **kwargs)
                    response_text, payload = _coerce_response(result, args, kwargs)
                    sigma, verdict = g.score(prompt, response_text)
                    elapsed_ms = (time.monotonic() - t0) * 1000.0
                return {
                    "result": payload,
                    "sigma": float(sigma),
                    "verdict": str(verdict),
                    "elapsed_ms": round(elapsed_ms, 2),
                }

            prompt2 = str(args[0]) if args else ""
            raw = fn(*args, **kwargs)
            text = raw if isinstance(raw, str) else str(raw)
            sigma, verdict = g.score(prompt2, text)
            if verdict == "ABSTAIN" and raise_on_abstain:
                raise SigmaAbstainError(f"σ-gate ABSTAIN (σ={sigma:.3f})")
            return SigmaResult(text, sigma, verdict)

        return wrapper  # type: ignore[return-value]

    if func is not None:
        return decorator(func)
    return decorator


def sigma_gated_llm(gate: Optional[Any] = None) -> Callable[[F], F]:
    """Dict-returning ``@…`` wrapper (v152 / LangChain-style integrations)."""
    return sigma_gated(return_dict=True, gate=gate, on_abstain="return")


__all__ = ["SigmaResult", "SigmaAbstainError", "sigma_gated", "sigma_gated_llm"]
