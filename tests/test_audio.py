# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.audio`."""
from __future__ import annotations

from cos.audio import SigmaAudio
from cos.sigma_gate import SigmaGate


def test_score_stt() -> None:
    a = SigmaAudio()
    r = a.score_stt(b"0123" * 200, "hello world", SigmaGate())
    assert "sigma_transcription" in r


def test_score_tts_empty_audio() -> None:
    a = SigmaAudio()
    r = a.score_tts("hi", b"", SigmaGate())
    assert r["sigma_synthesis"] > 0.3


def test_speaker_and_noise() -> None:
    assert "sigma" in SigmaAudio.speaker_sigma(b"a" * 500)
    assert "sigma" in SigmaAudio.noise_level_sigma(None)


def test_realtime_stream_sigma() -> None:
    a = SigmaAudio()
    chunks = iter([b"a" * 50, b"b" * 50])
    rows = a.realtime_stream_sigma(chunks, SigmaGate(), transcript_so_far="")
    assert len(rows) == 2


def test_language_detect_stub() -> None:
    d = SigmaAudio.language_detect(b"")
    assert "language" in d and "sigma_confidence" in d
