# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import pytest

from cos.voice_local import ABSTAIN_SPOKEN, SigmaVoice, listen, sigma_before_speak, speak


def test_listen_returns_sigma_mock_whisper(monkeypatch) -> None:
    class _Seg:
        def __init__(self, t: str) -> None:
            self.text = t

    class _Info:
        language_probability = 0.99

    class _StubSTT:
        def transcribe(self, *_a, **_k):
            return [_Seg("hello"), _Seg("world")], _Info()

    def _fake_init(self, *_a, **_k):
        self._stt = _StubSTT()
        self.gate = __import__("cos.sigma_gate", fromlist=["SigmaGate"]).SigmaGate()
        self.lang = "en"
        self.kokoro_voice = "af_bella"
        self.kokoro_lang_code = "a"
        self._tts = None

    monkeypatch.setattr("cos.voice._HAS_WHISPER", True)
    monkeypatch.setattr(SigmaVoice, "__init__", _fake_init)
    v = SigmaVoice()
    out = v.listen(audio_path="/dev/null/unused")
    assert "hello" in out["text"] and "world" in out["text"]
    assert "sigma" in out and 0.0 <= out["sigma"] <= 1.0
    assert out["verdict"] in ("ACCEPT", "RETHINK", "ABSTAIN")


def test_listen_empty_audio_abstains() -> None:
    out = listen(mock_text="")
    assert out["text"] == ""
    assert out["verdict"] == "ABSTAIN"
    assert out["sigma_transcription"] == 1.0


def test_sigma_before_speak_accept() -> None:
    out = sigma_before_speak("The capital of France is Paris.", prompt="geo")
    assert out["verdict"] in ("ACCEPT", "RETHINK", "ABSTAIN")
    assert out["speak_text"]
    assert isinstance(out["sigma"], float)


def test_sigma_before_speak_abstain_safe_phrase(monkeypatch) -> None:
    from cos import sigma_gate as sg

    def fake_score(self, p, r, reference=None):
        return 1.0, "ABSTAIN"

    monkeypatch.setattr(sg.SigmaGate, "score", fake_score)
    out = sigma_before_speak("anything")
    assert out["speak_text"] == ABSTAIN_SPOKEN
    assert out["allow"] is False


def test_voice_without_whisper_raises_import(monkeypatch, tmp_path) -> None:
    monkeypatch.setattr("cos.voice._HAS_WHISPER", False)
    p = tmp_path / "x.wav"
    p.write_bytes(b"RIFF")
    v = SigmaVoice.__new__(SigmaVoice)
    v.gate = __import__("cos.sigma_gate", fromlist=["SigmaGate"]).SigmaGate()
    v.language = "en"
    v.kokoro_voice = "af_bella"
    v.kokoro_lang_code = "a"
    v._stt = None
    v._tts = None
    with pytest.raises(ImportError, match="faster-whisper"):
        v.listen(audio_path=str(p))


def test_speak_with_play_fn() -> None:
    captured: list[str] = []
    out = speak("hi", play_fn=lambda s: captured.append(s))
    assert out["played"] is True
    assert captured == [out["speak_text"]]
