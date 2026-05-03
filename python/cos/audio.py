# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-audio — STT/TTS **lab** σ helpers (no DSP; wire samples from your codec).

Uses :class:`cos.sigma_gate.SigmaGate` on text legs; audio bytes only feed stub noise priors."""
from __future__ import annotations

import math
from typing import Any, Dict, Iterator, List, Union

__all__ = ["SigmaAudio"]

AudioChunk = Union[bytes, bytearray, None]


class SigmaAudio:
    """Score transcriptions and synthetic audio captions; chunk stream stubs."""

    @staticmethod
    def _bytes_energy(audio: AudioChunk) -> float:
        if not audio:
            return 0.0
        b = bytes(audio)
        if not b:
            return 0.0
        return min(1.0, math.log1p(len(b)) / 14.0)

    def score_stt(self, audio: AudioChunk, transcription: str, gate: Any) -> Dict[str, Any]:
        sigma_t, verdict = gate.score("stt_transcription", str(transcription))
        noise = self.noise_level_sigma(audio)
        sigma = min(1.0, float(sigma_t) * 0.65 + float(noise["sigma"]) * 0.35)
        return {
            "sigma_transcription": round(float(sigma), 6),
            "verdict": str(verdict),
            "noise_model": noise,
            "disclaimer": "σ_transcription mixes gate + length proxy — calibrate on your corpus.",
        }

    def score_tts(self, text: str, audio_output: AudioChunk, gate: Any) -> Dict[str, Any]:
        # TTS quality proxy: gate text + whether non-empty audio returned
        sigma0, verdict = gate.score("tts_target_text", str(text))
        empty = 0.85 if not audio_output else 0.1
        sigma = min(1.0, float(sigma0) * 0.5 + empty * 0.5)
        return {"sigma_synthesis": round(float(sigma), 6), "verdict": str(verdict)}

    @classmethod
    def speaker_sigma(cls, audio: AudioChunk) -> Dict[str, Any]:
        e = cls._bytes_energy(audio)
        conf = 1.0 - abs(e - 0.35)
        sigma = 1.0 - max(0.0, min(1.0, conf))
        return {"sigma": round(float(sigma), 6), "speaker_confidence_proxy": round(float(conf), 6)}

    @classmethod
    def noise_level_sigma(cls, audio: AudioChunk) -> Dict[str, Any]:
        e = cls._bytes_energy(audio)
        sigma = max(0.0, min(1.0, 0.95 - e))
        return {"sigma": round(float(sigma), 6), "energy_proxy": round(float(e), 6)}

    def realtime_stream_sigma(
        self,
        audio_stream: Iterator[AudioChunk],
        gate: Any,
        *,
        transcript_so_far: str = "",
    ) -> List[Dict[str, Any]]:
        out: List[Dict[str, Any]] = []
        for i, chunk in enumerate(audio_stream):
            partial = f"{transcript_so_far} [chunk:{i}]"
            s, v = gate.score("stream_partial", partial)
            n = self.noise_level_sigma(chunk)
            comb = min(1.0, float(s) * 0.7 + float(n["sigma"]) * 0.3)
            out.append({"chunk": i, "sigma": round(comb, 6), "verdict": str(v)})
        return out

    @staticmethod
    def language_detect(audio: AudioChunk) -> Dict[str, Any]:
        del audio  # real ASR language id goes here
        return {
            "language": "und",
            "sigma_confidence": 0.55,
            "note": "Stub: return ASR LID + calibrated σ in deployment.",
        }
