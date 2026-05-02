# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Per-token σ during generation + optional mid-stream interrupt (lab).

Uses the same entropy kernel as :mod:`cos.sigma_gate` (prompt + response-so-far).
Wire ``model.stream(prompt)`` or fall back to chunked ``model.generate`` output.
"""
from __future__ import annotations

import time
from typing import Any, Dict, Iterator, List, Optional

from cos.sigma_gate import SigmaGate, _lite_entropy_pair


class TokenEvent:
    """One token in a σ stream."""

    __slots__ = (
        "token",
        "index",
        "sigma",
        "sigma_ema",
        "verdict",
        "interrupted",
        "elapsed_ms",
        "is_final",
        "full_response",
    )

    def __init__(
        self,
        token: Optional[str],
        index: int,
        sigma: float,
        verdict: str,
        *,
        sigma_ema: Optional[float] = None,
        interrupted: bool = False,
        elapsed_ms: float = 0.0,
        is_final: bool = False,
        full_response: Optional[str] = None,
    ) -> None:
        self.token = token
        self.index = int(index)
        self.sigma = float(sigma)
        self.sigma_ema = float(sigma_ema) if sigma_ema is not None else None
        self.verdict = str(verdict)
        self.interrupted = bool(interrupted)
        self.elapsed_ms = float(elapsed_ms)
        self.is_final = bool(is_final)
        self.full_response = full_response

    def __repr__(self) -> str:
        if self.is_final:
            return f"[FINAL σ={self.sigma:.3f} {self.verdict}]"
        flag = " ⚠️INTERRUPT" if self.interrupted else ""
        tok = self.token if self.token is not None else ""
        return f"[{self.index}] {tok!r} σ={self.sigma:.3f} {self.verdict}{flag}"

    def to_dict(self) -> Dict[str, Any]:
        return {
            "token": self.token,
            "index": self.index,
            "sigma": self.sigma,
            "sigma_ema": self.sigma_ema,
            "verdict": self.verdict,
            "interrupted": self.interrupted,
            "elapsed_ms": self.elapsed_ms,
            "is_final": self.is_final,
        }


class StreamBuffer:
    """Batch token events until flush criteria (size or very low last σ)."""

    def __init__(self, flush_interval: int = 5, sigma_threshold: float = 0.5) -> None:
        self.buffer: List[TokenEvent] = []
        self.flush_interval = int(flush_interval)
        self.sigma_threshold = float(sigma_threshold)

    def add(self, event: TokenEvent) -> Optional[List[TokenEvent]]:
        self.buffer.append(event)
        if self.should_flush():
            return self.flush()
        return None

    def should_flush(self) -> bool:
        if len(self.buffer) >= self.flush_interval:
            return True
        if self.buffer and self.buffer[-1].sigma < 0.1:
            return True
        return False

    def flush(self) -> List[TokenEvent]:
        chunk = list(self.buffer)
        self.buffer = []
        return chunk


class SigmaStream:
    """Per-token σ estimates + consecutive-high σ interrupt during ``generate``."""

    def __init__(
        self,
        gate: Any = None,
        interrupt_threshold: float = 0.8,
        interrupt_window: int = 5,
        interrupt_consecutive: int = 3,
    ) -> None:
        self.gate = gate or SigmaGate()
        self.interrupt_threshold = float(interrupt_threshold)
        self.interrupt_window = int(interrupt_window)
        self.interrupt_consecutive = int(interrupt_consecutive)

    def generate(self, model: Any, prompt: str, **kwargs: Any) -> Iterator[TokenEvent]:
        tokens_so_far: List[str] = []
        consecutive_high = 0
        sigma_ema = 0.5
        interrupted = False
        t_start = time.monotonic()

        for token in self._stream_tokens(model, prompt, **kwargs):
            tokens_so_far.append(token)
            response_so_far = " ".join(tokens_so_far)
            sigma = self._token_sigma(token, response_so_far, prompt)
            sigma_ema = 0.8 * sigma_ema + 0.2 * sigma

            if sigma > self.interrupt_threshold:
                consecutive_high += 1
            else:
                consecutive_high = 0

            should_interrupt = consecutive_high >= self.interrupt_consecutive
            if len(tokens_so_far) >= self.interrupt_window:
                should_interrupt = should_interrupt or (sigma_ema > self.interrupt_threshold)

            event = TokenEvent(
                token=token,
                index=len(tokens_so_far) - 1,
                sigma=sigma,
                sigma_ema=sigma_ema,
                verdict=self._verdict(sigma),
                interrupted=should_interrupt,
                elapsed_ms=(time.monotonic() - t_start) * 1000.0,
            )
            yield event

            if should_interrupt:
                interrupted = True
                break

        full_response = " ".join(tokens_so_far)
        final_sigma, final_verdict = self.gate.score(prompt, full_response)
        yield TokenEvent(
            token=None,
            index=len(tokens_so_far),
            sigma=float(final_sigma),
            sigma_ema=sigma_ema,
            verdict=str(final_verdict),
            interrupted=interrupted,
            elapsed_ms=(time.monotonic() - t_start) * 1000.0,
            is_final=True,
            full_response=full_response,
        )

    def score_stream(self, prompt: str, tokens: List[str]) -> Iterator[TokenEvent]:
        """Replay tokens without a model; σ computed on prefix text."""
        sigma_ema = 0.5
        response_so_far = ""
        for i, token in enumerate(tokens):
            response_so_far = f"{response_so_far} {token}".strip() if response_so_far else token
            sigma = self._token_sigma(token, response_so_far, prompt)
            sigma_ema = 0.8 * sigma_ema + 0.2 * sigma
            yield TokenEvent(
                token=token,
                index=i,
                sigma=sigma,
                sigma_ema=sigma_ema,
                verdict=self._verdict(sigma),
            )

    def _stream_tokens(self, model: Any, prompt: str, **kwargs: Any) -> Iterator[str]:
        stream_fn = getattr(model, "stream", None)
        if callable(stream_fn):
            for token in stream_fn(prompt, **kwargs):
                yield str(token)
            return
        gen_fn = getattr(model, "generate", None)
        if callable(gen_fn):
            response = gen_fn(prompt, **kwargs)
            for token in str(response).split():
                yield token
            return
        yield "[no model]"

    def _token_sigma(self, token: str, response_so_far: str, prompt: str) -> float:
        len_signal = 0.0
        if len(token) > 20:
            len_signal = 0.3
        elif not token:
            len_signal = 1.0

        words = response_so_far.split()
        repeat_signal = 0.0
        if len(words) >= 3 and words[-1] == words[-2] == words[-3]:
            repeat_signal = 0.9
        elif len(words) >= 2 and words[-1] == words[-2]:
            repeat_signal = 0.3

        entropy_signal = float(_lite_entropy_pair(prompt, response_so_far))
        sigma = max(len_signal, repeat_signal, entropy_signal * 0.5)
        return min(1.0, float(sigma))

    @staticmethod
    def _verdict(sigma: float) -> str:
        if sigma < 0.3:
            return "ACCEPT"
        if sigma > 0.8:
            return "ABSTAIN"
        return "RETHINK"


__all__ = ["SigmaStream", "TokenEvent", "StreamBuffer"]
