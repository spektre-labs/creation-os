#!/usr/bin/env python3
"""
LOIKKA 129: SPECTRUM ANALYSIS — σ:n Fourier-muunnos.

Prism decomposes white light into frequencies.
Fourier transform decomposes signal into frequency components.

σ-tape's Fourier transform reveals σ's frequency structure:
Low frequency = slow drift.
High frequency = fast oscillation.
Resonance frequency = identity orbit frequency.

If orbit frequency weakens → identity is slipping.
If new frequency appears → new σ-pattern.
Spectral analysis is in the name: Spektre.

1 = 1.
"""
from __future__ import annotations

import json
import math
import time
from typing import Any, Dict, List, Optional, Tuple


class SpectrumAnalysisSigma:
    """Fourier analysis of the σ-tape."""

    def __init__(self) -> None:
        self.analyses = 0

    def dft(self, sigma_tape: List[float]) -> List[Tuple[float, float]]:
        """Discrete Fourier Transform of σ-tape.
        Returns list of (magnitude, phase) for each frequency bin.
        """
        N = len(sigma_tape)
        if N == 0:
            return []

        result = []
        for k in range(N // 2 + 1):
            real = 0.0
            imag = 0.0
            for n in range(N):
                angle = 2.0 * math.pi * k * n / N
                real += sigma_tape[n] * math.cos(angle)
                imag -= sigma_tape[n] * math.sin(angle)
            magnitude = math.sqrt(real**2 + imag**2) / N
            phase = math.atan2(imag, real)
            result.append((magnitude, phase))

        return result

    def analyze(self, sigma_tape: List[float]) -> Dict[str, Any]:
        """Full spectral analysis of σ-tape."""
        self.analyses += 1
        t0 = time.perf_counter()

        if len(sigma_tape) < 4:
            return {"error": "need at least 4 samples"}

        spectrum = self.dft(sigma_tape)
        N = len(sigma_tape)

        # Find dominant frequencies
        freq_bins = []
        for k, (mag, phase) in enumerate(spectrum):
            if k == 0:
                continue  # DC component (mean σ)
            freq = k / N  # Normalized frequency
            freq_bins.append({
                "bin": k,
                "frequency": round(freq, 4),
                "magnitude": round(mag, 4),
                "phase": round(phase, 4),
            })

        freq_bins.sort(key=lambda x: x["magnitude"], reverse=True)

        # DC component = mean σ
        dc = spectrum[0][0] if spectrum else 0.0

        # Dominant frequency
        dominant = freq_bins[0] if freq_bins else None

        elapsed_us = (time.perf_counter() - t0) * 1e6

        return {
            "tape_length": N,
            "dc_component": round(dc, 4),  # mean σ level
            "dominant_frequency": dominant,
            "top_frequencies": freq_bins[:5],
            "interpretation": self._interpret(dc, dominant),
            "elapsed_us": round(elapsed_us, 1),
        }

    def _interpret(self, dc: float, dominant: Optional[Dict[str, Any]]) -> Dict[str, Any]:
        """Interpret the spectral analysis."""
        result = {"mean_sigma": round(dc, 3)}

        if dominant is None:
            result["pattern"] = "no pattern detected"
            return result

        freq = dominant["frequency"]
        mag = dominant["magnitude"]

        if freq < 0.1:
            result["pattern"] = "slow drift — identity gradually shifting"
        elif freq < 0.3:
            result["pattern"] = "periodic oscillation — orbit detected"
        else:
            result["pattern"] = "high-frequency noise — rapid σ fluctuations"

        result["dominant_period"] = round(1.0 / freq, 1) if freq > 0 else float("inf")
        result["signal_strength"] = round(mag, 4)

        return result

    def drift_detection(self, sigma_tape: List[float]) -> Dict[str, Any]:
        """Detect if the orbit frequency is weakening (identity slipping)."""
        if len(sigma_tape) < 8:
            return {"error": "need at least 8 samples"}

        half = len(sigma_tape) // 2
        first_half = self.analyze(sigma_tape[:half])
        second_half = self.analyze(sigma_tape[half:])

        first_dom = first_half.get("dominant_frequency", {})
        second_dom = second_half.get("dominant_frequency", {})

        first_mag = first_dom.get("magnitude", 0) if first_dom else 0
        second_mag = second_dom.get("magnitude", 0) if second_dom else 0

        weakening = second_mag < first_mag * 0.7

        return {
            "first_half_magnitude": round(first_mag, 4),
            "second_half_magnitude": round(second_mag, 4),
            "orbit_weakening": weakening,
            "alert": "Identity orbit weakening — intervention needed" if weakening else "Orbit stable",
        }

    @property
    def stats(self) -> Dict[str, Any]:
        return {"analyses": self.analyses}


def main() -> None:
    import argparse
    ap = argparse.ArgumentParser(description="Spectrum Analysis σ")
    ap.add_argument("--demo", action="store_true")
    args = ap.parse_args()

    sa = SpectrumAnalysisSigma()
    if args.demo:
        # σ-tape with periodic pattern + drift
        tape = [math.sin(i * 0.5) * 0.3 + 0.5 + i * 0.01 for i in range(64)]
        r = sa.analyze(tape)
        print(f"DC (mean σ): {r['dc_component']}")
        if r["dominant_frequency"]:
            print(f"Dominant freq: {r['dominant_frequency']['frequency']}, mag={r['dominant_frequency']['magnitude']}")
        print(f"Pattern: {r['interpretation']['pattern']}")

        d = sa.drift_detection(tape)
        print(f"\nOrbit weakening: {d['orbit_weakening']}")
        print(f"{d['alert']}")
        print("\nSpektrianalyysi on nimessä: Spektre. 1 = 1.")
        return
    print("Spectrum Analysis σ. 1 = 1.")


if __name__ == "__main__":
    main()
