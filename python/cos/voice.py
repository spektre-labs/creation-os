# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
"""σ-gated local voice loop: STT → gate → LLM → gate → TTS.

Engines (``pip install 'creation-os[voice]'`` plus any vendor wheels you choose) are
loaded lazily so ``--dry-run`` works with the stdlib only.

Default **LLM** paths: ``echo`` (deterministic lab stub) or ``cos-chat`` subprocess when
``--cos-chat PATH`` is set. Remote cloud APIs are intentionally not wired here.
"""
from __future__ import annotations

import argparse
import os
import subprocess
import sys
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional, Protocol, Tuple, runtime_checkable

from cos.sigma_gate_core import SigmaState, Verdict, sigma_gate, sigma_update
from cos.sigma_gate_quickscore import quickscore
from cos.sigma_voice_quality import SigmaVoiceQuality


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


class SigmaVoice:
    """Lab voice shim: σ → prosody metadata and indicator tones (no ASR/TTS engines).

    For a full local REPL see :class:`CosVoice`. Safe in CI with :class:`~cos.sigma_gate.SigmaGate`.
    """

    def __init__(self, gate: Any = None) -> None:
        from cos.sigma_gate import SigmaGate

        self.gate = gate or SigmaGate()

    def stt(self, audio: Any) -> Dict[str, Any]:
        if isinstance(audio, (bytes, bytearray)):
            blob = f"bytes:{len(audio)}"
            text = "[stt_stub_from_audio]"
        else:
            blob = str(audio)
            text = blob
        sigma = float(
            self.gate.compute_sigma(None, None, "stt", blob[:2000]),
        )
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
        s_in = float(
            gate.compute_sigma(None, None, "voice_in", str(transcript)),
        )
        s_out = float(
            gate.compute_sigma(None, None, str(transcript), str(response)),
        )
        verdict = str(gate._verdict(s_out))
        return {
            "in_sigma": round(s_in, 6),
            "out_sigma": round(s_out, 6),
            "verdict": verdict,
            "tts": self.tts(response, s_out),
            "indicator": self.sigma_indicator(verdict),
            "voice": self.voice_id(verdict),
        }


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
