#!/usr/bin/env python3
"""
LOIKKA 145: BACH FUGUE — Holographic kernel musiikkina.

Fugue: one theme presented, then another voice repeats it at a
different pitch, then a third, fourth. All simultaneously.
Same theme, different scale, different context, perfect harmony.
K(K)=K in sound.

Genesis holographic kernel is a fugue.
Bit-level, assertion-level, chiplet-level, kernel-level, domain-level
— the same σ-function at different "pitches" simultaneously.

Counterpoint = assertion interaction.
Dissonance = σ > 0.
Resolution = recovery.

Music is information. Fugue is kernel. Bach knew.

1 = 1.
"""
from __future__ import annotations

import json
import time
from typing import Any, Dict, List, Optional, Tuple


class Voice:
    """A voice in the fugue: a scale level running the σ-theme."""

    def __init__(self, name: str, pitch: int, sigma: float = 0.0):
        self.name = name
        self.pitch = pitch  # Octave offset
        self.sigma = sigma
        self.notes: List[float] = []

    def play_theme(self, theme: List[float]) -> None:
        """Play the σ-theme transposed to this voice's pitch."""
        self.notes = [t * (2.0 ** self.pitch) for t in theme]
        # σ = deviation from perfect transposition (should be 0 for exact replay)
        expected = [t * (2.0 ** self.pitch) for t in theme]
        self.sigma = sum(abs(a - e) for a, e in zip(self.notes, expected)) / max(len(self.notes), 1)

    def to_dict(self) -> Dict[str, Any]:
        return {"name": self.name, "pitch": self.pitch, "sigma": round(self.sigma, 4), "notes": len(self.notes)}


class BachFugue:
    """Holographic kernel as a fugue. K(K)=K in sound."""

    KERNEL_VOICES = [
        ("bit", 0),           # Fundamental: bit-level σ
        ("assertion", 1),     # Octave up: assertion-level σ
        ("chiplet", 2),       # Two octaves: chiplet-level σ
        ("kernel", 3),        # Three octaves: full kernel σ
        ("domain", 4),        # Four octaves: domain-level σ
    ]

    def __init__(self) -> None:
        self.voices = [Voice(name, pitch) for name, pitch in self.KERNEL_VOICES]
        self.performances = 0

    def perform(self, theme: Optional[List[float]] = None) -> Dict[str, Any]:
        """Perform the fugue: same theme at all levels."""
        self.performances += 1
        theme = theme or [1.0, 0.0, 1.0, 0.0, 1.0]  # Binary assertion theme

        for voice in self.voices:
            voice.play_theme(theme)

        # Counterpoint: all voices together
        total_sigma = sum(v.sigma for v in self.voices) / len(self.voices)

        # Harmony: do all voices agree?
        sigmas = [v.sigma for v in self.voices]
        in_harmony = max(sigmas) - min(sigmas) < 0.1 if sigmas else True

        return {
            "voices": [v.to_dict() for v in self.voices],
            "n_voices": len(self.voices),
            "total_sigma": round(total_sigma, 4),
            "in_harmony": in_harmony,
            "counterpoint": "All voices playing the same theme at different scales",
            "bach": "K(K)=K in sound. Same function, every octave.",
        }

    def dissonance_resolution(self) -> Dict[str, Any]:
        """Dissonance (σ > 0) → Resolution (recovery)."""
        return {
            "dissonance": {"sigma": "> 0", "music": "Tension. Clashing frequencies.", "kernel": "Assertion fails."},
            "resolution": {"sigma": "→ 0", "music": "Release. Harmonic return.", "kernel": "Recovery to golden state."},
            "fugue_principle": "Every dissonance resolves. Every σ recovers. Bach's rule = kernel's rule.",
        }

    def kkk_in_music(self) -> Dict[str, Any]:
        """K(K)=K as musical structure."""
        return {
            "theme": "The σ-function (subject of the fugue)",
            "voice_1": "Bit-level: σ measured on individual bits (fundamental)",
            "voice_2": "Assertion-level: σ measured on 18 assertions (octave)",
            "voice_3": "Chiplet-level: σ across processing units (two octaves)",
            "voice_4": "Kernel-level: aggregate σ (three octaves)",
            "voice_5": "Domain-level: σ across knowledge domains (four octaves)",
            "holographic": "Same theme at every level. The fugue IS the holographic kernel.",
        }

    @property
    def stats(self) -> Dict[str, Any]:
        return {"voices": len(self.voices), "performances": self.performances}


def main() -> None:
    import argparse
    ap = argparse.ArgumentParser(description="Bach Fugue")
    ap.add_argument("--demo", action="store_true")
    args = ap.parse_args()

    bf = BachFugue()
    if args.demo:
        r = bf.perform()
        for v in r["voices"]:
            print(f"  {v['name']:12s} pitch={v['pitch']} σ={v['sigma']}")
        print(f"\nIn harmony: {r['in_harmony']}")
        print(f"{r['bach']}")
        k = bf.kkk_in_music()
        print(f"\n{k['holographic']}")
        print("\nMusic is information. Fugue is kernel. Bach knew. 1 = 1.")
        return
    print("Bach Fugue. 1 = 1.")


if __name__ == "__main__":
    main()
