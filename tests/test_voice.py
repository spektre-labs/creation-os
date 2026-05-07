# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import pytest

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


def test_available_always_gate() -> None:
    av = SigmaVoice().available()
    assert av["gate"] is True
    assert "stt" in av and "tts" in av


def test_transcribe_without_whisper_errors(monkeypatch) -> None:
    from cos import voice as voice_mod

    monkeypatch.setattr(voice_mod, "_HAS_WHISPER", False)
    bad = object.__new__(SigmaVoice)
    bad.gate = SigmaGate()
    bad.lang = "en"
    bad._stt = None
    out = SigmaVoice.transcribe(bad, "/noop.wav")  # type: ignore[arg-type]
    assert out.get("error") == "whisper not installed"
    assert out.get("sigma") == 1.0


def test_synthesize_without_kokoro_errors(monkeypatch) -> None:
    from cos import voice as voice_mod

    monkeypatch.setattr(voice_mod, "_HAS_KOKORO", False)
    bad = object.__new__(SigmaVoice)
    bad.gate = SigmaGate()
    bad.kokoro_voice = "af_heart"
    bad._tts = None
    out = SigmaVoice.synthesize(bad, "hello")  # type: ignore[arg-type]
    assert out.get("error") == "kokoro not installed"


def test_process_voice_pipeline(monkeypatch) -> None:
    sv = SigmaVoice()

    def _fake_transcribe(_path: str):
        return {"text": "hello there", "sigma": 0.2, "verdict": "ACCEPT", "σ": 0.2}

    def _fake_synth(text: str, output_path: str = "output.wav"):
        return {"path": "/tmp/voice_unit.wav", "sigma": 0.15, "verdict": "ACCEPT"}

    monkeypatch.setattr(sv, "transcribe", _fake_transcribe)
    monkeypatch.setattr(sv, "synthesize", _fake_synth)
    out = sv.process_voice("any.wav", respond_fn=lambda u: f"echo:{u}")
    assert out["input_text"] == "hello there"
    assert "echo:" in out["response_text"]
    assert out["sigma_output"] is not None


def test_voice_status() -> None:
    import json
    import os
    import subprocess
    import sys
    from pathlib import Path

    repo = Path(__file__).resolve().parents[1]
    env = {**os.environ, "PYTHONPATH": str(repo / "python")}
    r = subprocess.run(
        [sys.executable, "-m", "cos", "voice", "status", "--json"],
        cwd=str(repo),
        env=env,
        capture_output=True,
        text=True,
        check=False,
    )
    assert r.returncode == 0, r.stderr
    body = json.loads(r.stdout.strip())
    assert body.get("gate") is True
