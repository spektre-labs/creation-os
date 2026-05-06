# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Local voice — optional faster-whisper (STT) + Kokoro (TTS) with σ on transcript and before speak."""
from __future__ import annotations

import os
from typing import Any, Callable, Dict, List, Optional

from cos.sigma_gate import SigmaGate

try:
    from faster_whisper import WhisperModel
except ImportError:  # pragma: no cover - optional extra
    WhisperModel = None  # type: ignore[misc, assignment]

try:
    from kokoro import KPipeline  # type: ignore[import-not-found]
except ImportError:  # pragma: no cover
    KPipeline = None  # type: ignore[misc, assignment]

_HAS_WHISPER = WhisperModel is not None
_HAS_KOKORO = KPipeline is not None

__all__ = [
    "SigmaVoice",
    "listen",
    "sigma_before_speak",
    "speak",
    "voice_chat_turn",
]

ABSTAIN_SPOKEN = "I don't have a reliable answer for that."


def _verdict_str(verdict: object) -> str:
    if hasattr(verdict, "name"):
        return str(getattr(verdict, "name"))
    raw = str(verdict)
    if "." in raw:
        return raw.rsplit(".", 1)[-1]
    return raw


def _heuristic_transcription_sigma(text: str, language_probability: Optional[float]) -> float:
    t = (text or "").strip()
    if not t or len(t) < 3:
        return 1.0
    sigma = max(0.0, min(1.0, 1.0 - len(t) / 500.0))
    if language_probability is not None and language_probability < 0.8:
        sigma = min(1.0, sigma + 0.3)
    return sigma


def _verdict_from_sigma(sigma: float, gate: SigmaGate) -> str:
    ta = float(gate.threshold_accept)
    tb = float(gate.threshold_abstain)
    if sigma < ta:
        return "ACCEPT"
    if sigma > tb:
        return "ABSTAIN"
    return "RETHINK"


class SigmaVoice:
    """faster-whisper STT + Kokoro TTS + :class:`~cos.sigma_gate.SigmaGate` (all deps optional)."""

    def __init__(
        self,
        gate: Any = None,
        *,
        whisper_model: str = "tiny",
        kokoro_voice: str = "af_bella",
        language: str = "en",
        kokoro_lang_code: str = "a",
        device: Optional[str] = None,
        compute_type: str = "int8",
    ) -> None:
        self.gate = gate or SigmaGate()
        self.language = language
        self.kokoro_voice = kokoro_voice
        self.kokoro_lang_code = kokoro_lang_code
        self._stt: Any = None
        self._tts: Any = None
        if _HAS_WHISPER:
            dev = device or os.environ.get("COS_VOICE_WHISPER_DEVICE", "cpu")
            self._stt = WhisperModel(whisper_model, device=dev, compute_type=compute_type)
        if _HAS_KOKORO:
            self._tts = KPipeline(lang_code=kokoro_lang_code)

    def listen(
        self,
        *,
        audio_path: Optional[str] = None,
        audio_array: Any = None,
        sample_rate: int = 16000,
    ) -> Dict[str, Any]:
        """Transcribe audio to text + heuristic transcription σ + gate-style verdict."""
        if not _HAS_WHISPER or self._stt is None:
            raise ImportError(
                "faster-whisper not installed. Install: pip install 'creation-os[voice]'"
            )
        if audio_path:
            segments, info = self._stt.transcribe(
                str(audio_path),
                language=self.language or None,
                vad_filter=True,
            )
        elif audio_array is not None:
            segments, info = self._stt.transcribe(
                audio_array,
                language=self.language or None,
                vad_filter=True,
            )
        else:
            raise ValueError("Provide audio_path or audio_array")

        text = " ".join(seg.text.strip() for seg in segments)

        if not text.strip() or len(text.strip()) < 3:
            return {"text": "", "sigma": 1.0, "verdict": "ABSTAIN", "sigma_transcription": 1.0}

        lang_prob = getattr(info, "language_probability", None)
        sigma = _heuristic_transcription_sigma(text, lang_prob)
        verdict = _verdict_from_sigma(sigma, self.gate)
        return {
            "text": text.strip(),
            "sigma": round(float(sigma), 4),
            "verdict": verdict,
            "sigma_transcription": round(float(sigma), 4),
        }

    def speak(self, text: str, output_path: Optional[str] = None, *, sample_rate: int = 24000) -> Any:
        """Synthesize speech (numpy vector) and optionally write WAV via soundfile."""
        if not _HAS_KOKORO or self._tts is None:
            raise ImportError("kokoro not installed. Install: pip install 'creation-os[voice]'")
        samples: List[Any] = []
        for _, _, audio in self._tts(str(text), voice=self.kokoro_voice):
            samples.append(audio)
        if not samples:
            return None
        try:
            import numpy as np
        except ImportError as exc:  # pragma: no cover
            raise ImportError("TTS concat needs numpy: pip install numpy") from exc
        audio_out = np.concatenate(samples)
        if output_path:
            try:
                import soundfile as sf  # type: ignore[import-not-found]
            except ImportError as exc:  # pragma: no cover
                raise ImportError("Writing WAV needs soundfile: pip install soundfile") from exc
            sf.write(str(output_path), audio_out, sample_rate)
        return audio_out

    def sigma_before_speak(self, text: str, prompt: str = "") -> Dict[str, Any]:
        """σ-gate before TTS. ABSTAIN → safe spoken line; RETHINK → hedged line."""
        sigma, verdict = self.gate.score(prompt or "voice output", str(text))
        vn = _verdict_str(verdict)
        out: Dict[str, Any] = {"text": str(text), "sigma": float(sigma), "verdict": vn}
        if vn == "ACCEPT":
            out["speak_text"] = str(text)
        elif vn == "RETHINK":
            out["speak_text"] = f"I'm not fully certain, but: {text}"
        else:
            out["speak_text"] = ABSTAIN_SPOKEN
        out["sigma"] = round(float(sigma), 4)
        return out

    def voice_chat_turn(
        self,
        audio_path: str,
        chat_fn: Callable[[str], str],
        *,
        prompt_context: str = "",
    ) -> Dict[str, Any]:
        """listen → chat_fn → σ before speak → optional Kokoro audio."""
        transcription = self.listen(audio_path=audio_path)
        if transcription.get("verdict") == "ABSTAIN":
            return {**transcription, "response": None, "audio": None}

        user_text = str(transcription["text"])
        response_text = chat_fn(user_text)
        speak_result = self.sigma_before_speak(response_text, prompt=prompt_context or user_text)

        audio: Any = None
        if _HAS_KOKORO and self._tts is not None:
            try:
                audio = self.speak(speak_result["speak_text"])
            except ImportError:
                audio = None

        return {
            "user_text": user_text,
            "transcription_sigma": transcription.get("sigma"),
            "response_text": response_text,
            "response_sigma": speak_result["sigma"],
            "response_verdict": speak_result["verdict"],
            "speak_text": speak_result["speak_text"],
            "audio": audio,
        }


def listen(
    *,
    audio_path: Optional[str] = None,
    mock_text: Optional[str] = None,
) -> Dict[str, Any]:
    """
    Transcript + σ (portable path uses :class:`SigmaGate` on ``mock_text``; file path needs faster-whisper).
    """
    gate = SigmaGate()
    text = (mock_text if mock_text is not None else "").strip()
    if audio_path and not text:
        voice = SigmaVoice(gate=gate)
        out = voice.listen(audio_path=audio_path)
        return {
            "text": out.get("text", ""),
            "sigma_transcription": out.get("sigma_transcription", out.get("sigma", 0.0)),
            "verdict": out.get("verdict", "RETHINK"),
        }
    if not text or len(text) < 3:
        return {"text": "", "sigma_transcription": 1.0, "verdict": "ABSTAIN"}
    sigma, verdict = gate.score("transcription", text)
    return {
        "text": text,
        "sigma_transcription": round(float(sigma), 4),
        "verdict": _verdict_str(verdict),
    }


def sigma_before_speak(
    text: str,
    gate: Any = None,
    *,
    prompt: str = "",
) -> Dict[str, Any]:
    """σ-gate before speak; ABSTAIN yields a safe phrase for TTS."""
    sv = SigmaVoice(gate=gate or SigmaGate())
    inner = sv.sigma_before_speak(str(text), prompt=prompt)
    safe = str(inner["speak_text"])
    return {
        "speak_text": safe,
        "original": text,
        "sigma": inner["sigma"],
        "verdict": inner["verdict"],
        "allow": inner["verdict"] != "ABSTAIN",
    }


def speak(
    text: str,
    *,
    gate: Any = None,
    engine: str = "sigma",
    play_fn: Optional[Callable[[str], None]] = None,
    output_path: Optional[str] = None,
) -> Dict[str, Any]:
    """Speak after :func:`sigma_before_speak` (``kokoro`` optional; ``play_fn`` for tests)."""
    pre = sigma_before_speak(text, gate=gate)
    phrase = str(pre["speak_text"])
    if play_fn is not None:
        play_fn(phrase)
        return {**pre, "played": True, "engine": "callback"}
    if engine == "kokoro":
        voice = SigmaVoice(gate=gate)
        audio = voice.speak(phrase, output_path=output_path)
        return {**pre, "played": audio is not None or bool(output_path), "engine": "kokoro", "audio": audio}
    return {**pre, "played": False, "engine": engine or "none"}


def voice_chat_turn(
    audio_path: str,
    chat_fn: Callable[[str], str],
    *,
    gate: Any = None,
    prompt_context: str = "",
    **kwargs: Any,
) -> Dict[str, Any]:
    """Module-level wrapper: :meth:`SigmaVoice.voice_chat_turn`."""
    sv = SigmaVoice(gate=gate, **kwargs)
    return sv.voice_chat_turn(audio_path, chat_fn, prompt_context=prompt_context)
