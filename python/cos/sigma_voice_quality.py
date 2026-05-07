# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
"""Map ASR segment confidences to a σ-like urgency score (lab / voice UX).

Higher σ ⇒ noisier or less confident transcription ⇒ prefer asking the user to repeat.

This does **not** replace the LSD trajectory probe; it is a lightweight STT-side signal
compatible with :mod:`cos.sigma_gate_core` updates.
"""
from __future__ import annotations

from typing import Any, Iterable, List, Protocol, runtime_checkable


def _f(x: Any) -> float:
    try:
        return float(x)
    except (TypeError, ValueError):
        return 0.0


@runtime_checkable
class _SegmentLike(Protocol):
    avg_logprob: Any


class SigmaVoiceQuality:
    """Whisper / faster-whisper style segments → σ in [0, 1]."""

    def __init__(self, retry_tau: float = 0.5) -> None:
        self.retry_tau = float(retry_tau)

    @staticmethod
    def stt_sigma_from_segments(segments: Iterable[Any]) -> float:
        """Average ``avg_logprob`` over segments → σ (no segments ⇒ 1.0)."""
        vals: List[float] = []
        for seg in segments:
            lp = getattr(seg, "avg_logprob", None)
            if lp is None:
                continue
            vals.append(_f(lp))
        if not vals:
            return 1.0
        avg_conf = sum(vals) / float(len(vals))
        # Typical log-probs are negative; 0 is very confident.
        sigma = max(0.0, min(1.0, -avg_conf))
        return float(sigma)

    def should_retry(self, stt_sigma: float) -> bool:
        return float(stt_sigma) > self.retry_tau


__all__ = ["SigmaVoiceQuality"]
