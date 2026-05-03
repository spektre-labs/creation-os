# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from cos.sigma_gate import SigmaGate
from cos.voice import SigmaVoice


def test_stt_bytes_and_sigma() -> None:
    sv = SigmaVoice()
    out = sv.stt(b"\x00\x01\xff")
    assert "text" in out and "sigma_transcription" in out
    assert isinstance(out["sigma_transcription"], float)


def test_tts_prosody_low_vs_high_sigma() -> None:
    sv = SigmaVoice()
    steady = sv.tts("hello", 0.1)
    hesitant = sv.tts("hello", 0.5)
    assert steady["prosody"] == "steady"
    assert hesitant["prosody"] == "hesitant"


def test_sigma_indicator_mapping() -> None:
    sv = SigmaVoice()
    assert sv.sigma_indicator("ACCEPT") == "chime_low"
    assert sv.sigma_indicator("ABSTAIN") == "chime_high"


def test_voice_id_mapping() -> None:
    sv = SigmaVoice()
    assert "railo" in sv.voice_id("ACCEPT")
    assert sv.voice_id("unknown") == "default"


def test_realtime_stream_roundtrip_keys() -> None:
    sv = SigmaVoice()
    g = SigmaGate()
    r = sv.realtime_stream("What is 2+2?", g, "4")
    for k in ("in_sigma", "out_sigma", "verdict", "tts", "indicator", "voice"):
        assert k in r
