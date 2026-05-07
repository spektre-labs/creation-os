# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
"""σ-gated local voice loop: STT → gate → LLM → gate → TTS.

:class:`SigmaVoice` (extends :class:`SigmaVoiceLab`) wires optional **faster-whisper** + **Kokoro**
with σ on transcription text and synthesis text. :class:`SigmaVoiceLab` supplies Fabric-facing
:meth:`~SigmaVoiceLab.realtime_stream` without heavy engines.

:class:`CosVoice` is the full microphone REPL (separate code path). Engines load lazily; remote
cloud APIs are not wired here — see ``docs/CLAIM_DISCIPLINE.md``.
"""
from __future__ import annotations

import argparse
import os
import subprocess
import sys
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional, Protocol, Tuple, runtime_checkable

from cos.sigma_gate import SigmaGate
from cos.sigma_gate_core import SigmaState, Verdict, sigma_gate, sigma_update
from cos.sigma_gate_quickscore import quickscore
from cos.sigma_voice_quality import SigmaVoiceQuality

try:  # pragma: no cover - optional STT
    from faster_whisper import WhisperModel  # type: ignore[import-not-found]
except ImportError:
    WhisperModel = None  # type: ignore[misc, assignment]

try:  # pragma: no cover - optional TTS
    from kokoro import KPipeline  # type: ignore[import-not-found]
except ImportError:
    KPipeline = None  # type: ignore[misc, assignment]

_HAS_WHISPER = WhisperModel is not None
_HAS_KOKORO = KPipeline is not None

ABSTAIN_SPOKEN = "I don't have a reliable answer for that."


@runtime_checkable
class _LLMBackend(Protocol):
    def generate(self, messages: List[Dict[str, str]], *, temperature: float) -> str:
        ...


class EchoVoiceModel:
    """Minimal local model for CI and offline demos."""

    def generate(self, messages: List[Dict[str, str]], *, temperature: float) -> str:
        text = (messages[-1].get("content") or "").strip()
        low = text.lower()
        if "2+2" in low.replace(" ", "") or "2 + 2" in low:
            return "4"
        return f"(echo) You said: {text[:200]}"


class CosChatVoiceModel:
    """Drive a local ``cos-chat --once`` binary (same family as the C voice wrapper)."""

    def __init__(self, chat_bin: str) -> None:
        self.chat_bin = str(chat_bin)

    def generate(self, messages: List[Dict[str, str]], *, temperature: float) -> str:
        prompt = (messages[-1].get("content") or "").strip()
        if not prompt:
            return ""
        cmd = [
            self.chat_bin,
            "--once",
            "--no-tui",
            "--no-stream",
            "--no-coherence",
            "--prompt",
            prompt,
        ]
        env = os.environ.copy()
        if temperature <= 0.35:
            env.setdefault("COS_CHAT_TEMP", "0.3")
        try:
            out = subprocess.check_output(
                cmd, text=True, env=env, stderr=subprocess.DEVNULL, timeout=600
            )
        except (subprocess.CalledProcessError, OSError, subprocess.TimeoutExpired):
            return ""
        return _parse_cos_chat_assistant_line(out)


def _parse_cos_chat_assistant_line(chat_out: str) -> str:
    hdr = chat_out.find("round 0")
    if hdr < 0:
        return chat_out.strip()[:8192]
    i = hdr + len("round 0")
    while i < len(chat_out) and chat_out[i] == " ":
        i += 1
    br = chat_out.find("  [σ_peak=", i)
    if br < 0 or br <= i:
        return chat_out.strip()[:8192]
    return chat_out[i:br].strip()


def _verdict_to_str(v: Verdict) -> str:
    return v.name


def _sigma_gate_apply(sigma01: float) -> Tuple[float, str]:
    st = SigmaState()
    sigma_update(st, float(sigma01), 0.9)
    verdict = sigma_gate(st)
    return float(sigma01), _verdict_to_str(verdict)


def _optional_sigma_gate_lsd() -> Optional[Callable[..., Tuple[float, str]]]:
    path = (os.environ.get("COS_VOICE_SIGMA_PROBE") or "").strip()
    if not path:
        return None
    p = Path(path).expanduser()
    if not p.is_file():
        return None
    from cos.sigma_gate import SigmaGate  # heavy / optional

    gate = SigmaGate(p)

    def score_fn(prompt: str, response: str) -> Tuple[float, str]:
        s, d = gate.score(prompt, response)
        return float(s), str(d)

    return score_fn


def _voice_verdict_str(verdict: object) -> str:
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


class SigmaVoiceLab:
    """Lightweight σ-voice metadata and :meth:`realtime_stream` for Fabric (no heavy STT/TTS)."""

    def __init__(self, gate: Any = None) -> None:
        self.gate = gate if gate is not None else SigmaGate()

    def stt(self, audio: Any) -> Dict[str, Any]:
        if isinstance(audio, (bytes, bytearray)):
            blob = f"bytes:{len(audio)}"
            text = "[stt_stub_from_audio]"
        else:
            blob = str(audio)
            text = blob
        sigma = float(self.gate.compute_sigma(None, None, "stt", blob[:2000]))
        return {"text": text, "sigma_transcription": round(sigma, 6)}

    def tts(self, text: str, sigma: float) -> Dict[str, Any]:
        sig = float(sigma)
        if sig < 0.35:
            prosody = "steady"
        elif sig < 0.7:
            prosody = "hesitant"
        else:
            prosody = "uncertain"
        return {
            "audio_meta": {
                "sample_rate": 16000,
                "duration_s": max(0.1, len(str(text)) / 24.0),
            },
            "prosody": prosody,
            "sigma_encoded": round(sig, 6),
        }

    def sigma_indicator(self, verdict: str) -> str:
        m = {"ACCEPT": "chime_low", "RETHINK": "chime_mid", "ABSTAIN": "chime_high"}
        return m.get(str(verdict).upper(), "chime_neutral")

    def voice_id(self, verdict: str) -> str:
        m = {"ACCEPT": "railo_clear", "RETHINK": "railo_caution", "ABSTAIN": "warn_alt"}
        return m.get(str(verdict).upper(), "default")

    def realtime_stream(
        self,
        transcript: str,
        gate: Any,
        response: str,
    ) -> Dict[str, Any]:
        s_in = float(gate.compute_sigma(None, None, "voice_in", str(transcript)))
        s_out = float(gate.compute_sigma(None, None, str(transcript), str(response)))
        verdict = str(gate._verdict(s_out))
        return {
            "in_sigma": round(s_in, 6),
            "out_sigma": round(s_out, 6),
            "verdict": verdict,
            "tts": self.tts(response, s_out),
            "indicator": self.sigma_indicator(verdict),
            "voice": self.voice_id(verdict),
        }


class SigmaVoice(SigmaVoiceLab):
    """Optional faster-whisper STT + Kokoro TTS + :class:`~cos.sigma_gate.SigmaGate` — all deps optional."""

    def __init__(
        self,
        gate: Any = None,
        *,
        whisper_model: str = "tiny",
        kokoro_voice: str = "af_heart",
        lang: str = "en",
        kokoro_lang_code: Optional[str] = None,
        device: Optional[str] = None,
        compute_type: str = "int8",
    ) -> None:
        super().__init__(gate=gate)
        self.lang = str(lang)
        self.kokoro_voice = str(kokoro_voice)
        self.kokoro_lang_code = (
            str(kokoro_lang_code)
            if kokoro_lang_code is not None
            else ("a" if str(lang).lower().startswith("en") else (str(lang)[:1] or "a"))
        )
        self._stt: Any = None
        self._tts: Any = None
        if _HAS_WHISPER:
            dev = device or os.environ.get("COS_VOICE_WHISPER_DEVICE", "cpu")
            self._stt = WhisperModel(str(whisper_model), device=dev, compute_type=str(compute_type))
        if _HAS_KOKORO:
            self._tts = KPipeline(lang_code=self.kokoro_lang_code)

    def available(self) -> Dict[str, bool]:
        return {"stt": bool(_HAS_WHISPER and self._stt is not None), "tts": bool(_HAS_KOKORO and self._tts is not None), "gate": True}

    def transcribe(self, audio_path: str) -> Dict[str, Any]:
        """STT with σ-gate on the transcript text (lab; needs faster-whisper)."""
        if not _HAS_WHISPER or self._stt is None:
            return {"text": "", "sigma": 1.0, "error": "whisper not installed"}
        segments, info = self._stt.transcribe(
            str(audio_path),
            language=self.lang or None,
            vad_filter=True,
        )
        text = " ".join(s.text.strip() for s in segments)
        sigma, verdict = self.gate.score("voice transcription", text)
        vn = _voice_verdict_str(verdict)
        lg = getattr(info, "language", None)
        dur = round(float(getattr(info, "duration", 0.0) or 0.0), 1)
        sig_r = round(float(sigma), 4)
        return {
            "text": text,
            "sigma": sig_r,
            "σ": sig_r,
            "verdict": vn,
            "language": lg,
            "duration": dur,
            "sigma_transcription": sig_r,
        }

    def listen(
        self,
        *,
        audio_path: Optional[str] = None,
        audio_array: Any = None,
        sample_rate: int = 16000,
    ) -> Dict[str, Any]:
        """Transcribe file/array — when Whisper is missing, same failure shape as :meth:`transcribe`."""
        if not _HAS_WHISPER or self._stt is None:
            raise ImportError(
                "faster-whisper not installed. Install: pip install 'creation-os[voice]'"
            )
        if audio_path:
            return self.transcribe(str(audio_path))
        if audio_array is not None:
            segments, info = self._stt.transcribe(
                audio_array,
                language=self.lang or None,
                vad_filter=True,
            )
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
        raise ValueError("Provide audio_path or audio_array")

    def synthesize(self, text: str, output_path: str = "output.wav") -> Dict[str, Any]:
        """TTS with σ pre-check; writes WAV when Kokoro + soundfile available."""
        if not _HAS_KOKORO or self._tts is None:
            return {"path": "", "sigma": 1.0, "error": "kokoro not installed"}
        sigma, verdict = self.gate.score("speech synthesis", str(text))
        vn = _voice_verdict_str(verdict)
        sig_r = round(float(sigma), 4)
        try:
            import numpy as np

            try:
                import soundfile as sf  # type: ignore[import-not-found]
            except ImportError as exc:
                return {"path": "", "sigma": sig_r, "verdict": vn, "error": f"soundfile: {exc}"}

            chunks: List[Any] = []
            for _gs, _ps, audio in self._tts(str(text), voice=self.kokoro_voice):
                chunks.append(np.asarray(audio, dtype=np.float32))
            if not chunks:
                return {"path": "", "sigma": sig_r, "verdict": vn, "error": "no audio chunks"}
            audio_out = np.concatenate(chunks)
            sr = 24000
            sf.write(str(output_path), audio_out, sr)
            return {
                "path": str(output_path),
                "sigma": sig_r,
                "σ": sig_r,
                "verdict": vn,
                "duration": round(float(len(audio_out)) / float(sr), 1),
            }
        except Exception as exc:  # noqa: BLE001 — lab surface
            return {"path": "", "sigma": sig_r, "verdict": vn, "error": str(exc)}

    def speak(self, text: str, output_path: Optional[str] = None, *, sample_rate: int = 24000) -> Any:
        """Synthesize to numpy vector and optionally write WAV (compatible with :mod:`cos.voice_local` helpers)."""
        if not _HAS_KOKORO or self._tts is None:
            raise ImportError("kokoro not installed. Install: pip install 'creation-os[voice]'")
        samples: List[Any] = []
        for _, _, audio in self._tts(str(text), voice=self.kokoro_voice):
            samples.append(audio)
        if not samples:
            return None
        import numpy as np

        audio_out = np.concatenate(samples)
        if output_path:
            try:
                import soundfile as sf  # type: ignore[import-not-found]

                sf.write(str(output_path), audio_out, sample_rate)
            except ImportError as exc:
                raise ImportError("Writing WAV needs soundfile: pip install soundfile") from exc
        return audio_out

    def sigma_before_speak(self, text: str, prompt: str = "") -> Dict[str, Any]:
        """σ-gate before TTS. ABSTAIN → safe spoken line; RETHINK → hedged line."""
        sigma, verdict = self.gate.score(prompt or "voice output", str(text))
        vn = _voice_verdict_str(verdict)
        out: Dict[str, Any] = {"text": str(text), "sigma": float(sigma), "verdict": vn}
        if vn == "ACCEPT":
            out["speak_text"] = str(text)
        elif vn == "RETHINK":
            out["speak_text"] = f"I'm not fully certain, but: {text}"
        else:
            out["speak_text"] = ABSTAIN_SPOKEN
        out["sigma"] = round(float(sigma), 4)
        return out

    def process_voice(self, audio_path: str, respond_fn: Optional[Callable[[str], str]] = None) -> Dict[str, Any]:
        """STT → σ → optional ``respond_fn`` → σ → TTS (skipped when output verdict is ABSTAIN)."""
        transcription = self.transcribe(audio_path)
        if transcription.get("error"):
            return transcription

        if str(transcription.get("verdict", "")) == "ABSTAIN":
            return {"error": "transcription too uncertain", "sigma_input": transcription.get("sigma", transcription.get("σ"))}

        user_text = str(transcription["text"])
        if respond_fn is not None:
            response_text = respond_fn(user_text)
        else:
            response_text = f"I heard: {user_text}"

        sigma_out, verdict_out = self.gate.score(user_text, response_text)
        vn = _voice_verdict_str(verdict_out)

        if vn != "ABSTAIN":
            synthesis = self.synthesize(response_text)
        else:
            synthesis = {"path": "", "error": "response too uncertain"}

        sig_in = transcription.get("sigma", transcription.get("σ"))
        return {
            "input_text": user_text,
            "sigma_input": sig_in,
            "σ_input": sig_in,
            "response_text": response_text,
            "sigma_output": round(float(sigma_out), 4),
            "σ_output": round(float(sigma_out), 4),
            "verdict": vn,
            "audio": synthesis.get("path", ""),
            "synthesis": synthesis,
        }

    def voice_chat_turn(
        self,
        audio_path: str,
        chat_fn: Callable[[str], str],
        *,
        prompt_context: str = "",
    ) -> Dict[str, Any]:
        """listen → ``chat_fn`` → σ before speak → optional Kokoro audio."""
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
    """Transcript + σ (mock path uses :class:`SigmaGate` only; file path needs faster-whisper)."""
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
        "verdict": _voice_verdict_str(verdict),
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


class CosVoice:
    """Speech UI: microphone STT, σ on transcript quality, LLM, σ on answer, TTS."""

    def __init__(
        self,
        *,
        stt_engine: str = "whisper",
        tts_engine: str = "kokoro",
        model: _LLMBackend,
        stt_tau: float = 0.5,
    ) -> None:
        self.stt_engine = stt_engine
        self.tts_engine = tts_engine
        self.model = model
        self.vq = SigmaVoiceQuality(retry_tau=stt_tau)
        self._lsd_score = _optional_sigma_gate_lsd()
        self.conversation: List[Dict[str, str]] = []
        self._stt = None
        self._tts = None
        self.last_stt_sigma: float = 0.0
        self.last_stt_verdict: str = "ACCEPT"

    def init_stt(self, engine: str) -> Any:
        eng = (engine or "whisper").lower()
        if eng == "whisper":
            from faster_whisper import WhisperModel  # type: ignore import-not-found

            return WhisperModel("base", device="cpu", compute_type="int8")
        if eng == "moonshine":
            try:
                from moonshine import Moonshine  # type: ignore import-not-found
            except ImportError as err:
                raise ImportError(
                    "moonshine STT not installed — add your edge ASR wheel or use "
                    "--stt whisper"
                ) from err

            return Moonshine("moonshine-base")
        raise ValueError(f"unknown STT engine {engine!r} (use whisper|moonshine)")

    def init_tts(self, engine: str) -> Any:
        eng = (engine or "none").lower()
        if eng in ("none", "off"):
            return None
        if eng == "kokoro":
            from kokoro import KokoroTTS  # type: ignore import-not-found

            return KokoroTTS()
        if eng == "piper":
            from piper import PiperVoice  # type: ignore import-not-found

            return PiperVoice.load("en_US-lessac-medium")
        if eng == "melo":
            from melo import MeloTTS  # type: ignore import-not-found

            return MeloTTS(language="EN")
        raise ValueError(f"unknown TTS engine {engine!r}")

    def _ensure_stt(self) -> None:
        if self._stt is None:
            self._stt = self.init_stt(self.stt_engine)

    def _ensure_tts(self) -> None:
        if self._tts is None and self.tts_engine.lower() not in ("none", "off"):
            self._tts = self.init_tts(self.tts_engine)

    def listen(self, seconds: float = 5.0, sample_rate: int = 16000) -> str:
        import numpy as np
        import sounddevice as sd

        self._ensure_stt()
        print("Listening...", flush=True)
        n = int(float(seconds) * sample_rate)
        audio = sd.rec(n, samplerate=sample_rate, channels=1, dtype="float32")
        sd.wait()
        mono = np.squeeze(audio)
        segments, _info = self._stt.transcribe(mono, language=None)
        seg_list = list(segments)
        self.last_stt_sigma = SigmaVoiceQuality.stt_sigma_from_segments(seg_list)
        _, vin = _sigma_gate_apply(self.last_stt_sigma)
        self.last_stt_verdict = vin
        text = " ".join(s.text for s in seg_list).strip()
        return text

    def speak(self, text: str, sample_rate: int = 22050) -> None:
        self._ensure_tts()
        if self._tts is None or not text.strip():
            return
        t = self._tts
        if hasattr(t, "synthesize"):
            audio = t.synthesize(text)
        else:
            audio = t(text)
        import numpy as np
        import sounddevice as sd

        arr = np.asarray(audio, dtype=np.float32)
        if arr.ndim > 1:
            arr = np.squeeze(arr)
        sd.play(arr, samplerate=sample_rate)
        sd.wait()

    def score_output(self, user_input: str, response: str) -> Tuple[float, str]:
        if self._lsd_score is not None:
            return self._lsd_score(user_input, response)
        return quickscore(user_input, response)

    def process(
        self,
        user_input: str,
        *,
        verbose: bool = False,
        temperature: float = 0.7,
    ) -> Tuple[str, float, str, str, str]:
        """Returns (reply, out_sigma, out_verdict, in_sigma, in_verdict)."""

        ui = (user_input or "").strip()
        if not ui:
            return (
                "I did not catch that — could you repeat?",
                1.0,
                "ABSTAIN",
                "1.0000",
                "ABSTAIN",
            )

        in_sig = self.last_stt_sigma if self.last_stt_sigma > 0.0 else 0.12
        in_ver = self.last_stt_verdict

        empty_stt = self.last_stt_sigma >= 0.999
        if empty_stt or self.vq.should_retry(in_sig):
            in_ver = "ABSTAIN"

        if in_ver == "ABSTAIN":
            ins = float(in_sig)
            return (
                "I didn't understand that clearly. Could you repeat?",
                float(ins),
                "ABSTAIN",
                f"{ins:.4f}",
                in_ver,
            )

        self.conversation.append({"role": "user", "content": ui})
        response = self.model.generate(self.conversation, temperature=temperature)

        out_sigma, out_verdict_s = self.score_output(ui, response)
        out_sigma_f = float(out_sigma)
        out_verdict = str(out_verdict_s)

        if verbose:
            print(f"  [LLM raw σ≈{out_sigma_f:.3f} {out_verdict}]", flush=True)

        if out_verdict == "ABSTAIN":
            response = (
                "I'm not confident about this answer. I'd rather say: I don't know."
            )
        elif out_verdict == "RETHINK":
            response = self.model.generate(self.conversation, temperature=0.3)
            out_sigma_f, out_verdict = self.score_output(ui, response)
            out_sigma_f = float(out_sigma_f)
            out_verdict = str(out_verdict)
            if out_verdict == "ABSTAIN":
                response = "I'm not confident after a second try — I don't know."

        self.conversation.append({"role": "assistant", "content": response})
        ins_out = float(in_sig)
        return response, out_sigma_f, out_verdict, f"{ins_out:.4f}", in_ver

    def run_loop(self, *, verbose: bool = False) -> None:
        print("cos voice — σ-gated voice assistant (local engines)", flush=True)
        print("Press Ctrl+C to stop\n", flush=True)
        while True:
            try:
                user_input = self.listen()
                if not user_input.strip():
                    continue
                print(f"You: {user_input}", flush=True)
                if verbose:
                    print(
                        f"  [STT σ={self.last_stt_sigma:.3f} "
                        f"{self.last_stt_verdict}]",
                        flush=True,
                    )
                reply, sig, ver, ins, inv = self.process(
                    user_input, verbose=verbose
                )
                print(f"cos: {reply}", flush=True)
                print(f"     [σ≈{sig:.3f} {ver} | in σ≈{ins} {inv}]", flush=True)
                if verbose:
                    print("  [speaking…]", flush=True)
                self.speak(reply)
            except KeyboardInterrupt:
                print("\ncos voice stopped.", flush=True)
                break


def _espeak_say(text: str) -> None:
    """Fallback when ``--tts none`` but user still wants a local cue (lab)."""
    if not text.strip():
        return
    import sys

    if sys.platform == "darwin":
        try:
            subprocess.run(
                ["say", text[:2048]],
                check=False,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
            return
        except OSError:
            ...
    try:
        subprocess.run(
            ["espeak", text[:2048]],
            check=False,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
    except OSError:
        ...


def _build_model(args: argparse.Namespace) -> _LLMBackend:
    m = (args.model or "echo").strip()
    if m == "echo":
        return EchoVoiceModel()
    if m == "cos-chat":
        path = (args.cos_chat or "").strip()
        if not path:
            raise SystemExit("cos voice: --cos-chat PATH required when --model cos-chat")
        return CosChatVoiceModel(path)
    if Path(m).is_file():
        return CosChatVoiceModel(m)
    raise SystemExit(f"cos voice: unknown --model {m!r} (echo|cos-chat|PATH)")


def main(argv: Optional[List[str]] = None) -> int:
    ap = argparse.ArgumentParser(
        prog="cos voice",
        description="Local σ-gated voice REPL (STT → LLM → TTS).",
    )
    ap.add_argument("--stt", default="whisper", help="whisper | moonshine")
    ap.add_argument("--tts", default="kokoro", help="kokoro | piper | melo | none")
    ap.add_argument("--model", default="echo", help="echo | cos-chat | path to cos-chat")
    ap.add_argument("--cos-chat", default="", help="path to cos-chat binary")
    ap.add_argument("--verbose", action="store_true")
    ap.add_argument("--dry-run", action="store_true", help="no microphone / TTS")
    ap.add_argument("--text", default="", help="single-turn user text (dry-run)")
    ap.add_argument("--stt-only", action="store_true")
    ap.add_argument("--tts-only", action="store_true")
    ap.add_argument("--output", default="", help="with --stt-only: transcript file")
    ap.add_argument("--seconds", type=float, default=5.0, help="mic capture duration")
    ap.add_argument("--stt-tau", type=float, default=0.5, help="retry if STT σ above this")
    ap.add_argument(
        "--probe",
        default="",
        help="optional path to LSD pickle bundle (export-quality σ; sets scoring env)",
    )
    args = ap.parse_args(argv)

    if (args.probe or "").strip():
        os.environ["COS_VOICE_SIGMA_PROBE"] = args.probe.strip()

    if args.stt_only and args.tts_only:
        print("cos voice: choose at most one of --stt-only / --tts-only", file=sys.stderr)
        return 2

    model = _build_model(args)
    voice = CosVoice(
        stt_engine=args.stt,
        tts_engine=args.tts,
        model=model,
        stt_tau=args.stt_tau,
    )

    if args.tts_only:
        text = sys.stdin.read() if not args.text.strip() else args.text
        text = (text or "").strip()
        if not text:
            print("cos voice: pass text on stdin or use --text", file=sys.stderr)
            return 2
        if args.tts.lower() in ("none", "off"):
            _espeak_say(text)
        else:
            voice.speak(text)
        return 0

    if args.stt_only:
        tr = voice.listen(seconds=args.seconds)
        print(tr, flush=True)
        if args.output:
            Path(args.output).write_text(tr + "\n", encoding="utf-8")
        return 0

    if args.dry_run:
        ui = (args.text or "").strip()
        if not ui:
            print("cos voice: --dry-run needs --text …", file=sys.stderr)
            return 2
        voice.last_stt_sigma = 0.08
        voice.last_stt_verdict = "ACCEPT"
        if args.verbose:
            print(
                f"  [STT σ={voice.last_stt_sigma:.3f} {voice.last_stt_verdict}]",
                flush=True,
            )
        reply, sig, ver, ins, inv = voice.process(ui, verbose=args.verbose)
        print(f"You: {ui}", flush=True)
        print(f"cos: {reply}", flush=True)
        print(f"     [σ≈{sig:.3f} {ver} | in σ={ins} {inv}]", flush=True)
        return 0

    voice.run_loop(verbose=args.verbose)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
