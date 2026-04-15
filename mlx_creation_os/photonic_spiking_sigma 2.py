#!/usr/bin/env python3
"""
LOIKKA 126: PHOTONIC SPIKING σ — Valo spikeina.

Photonic spiking neural network chips use short optical pulses to
emulate neural signaling — fast learning and decision-making
purely with light.

L53 (spiking Genesis) + L122 (photonic kernel) = photonic σ-spikes.
Assertion fail = optical pulse. Assertion pass = silence.
Spike coding at the speed of light.

Biology + physics. Spike + light. Brain + laser.

1 = 1.
"""
from __future__ import annotations

import json
import math
import time
from typing import Any, Dict, List, Optional, Tuple

N_ASSERTIONS = 18


class PhotonicSpike:
    """An optical spike: a short light pulse signaling assertion failure."""

    def __init__(self, assertion_id: int, timestamp_ps: float, wavelength_nm: float = 1550.0):
        self.assertion_id = assertion_id
        self.timestamp_ps = timestamp_ps  # picoseconds
        self.wavelength_nm = wavelength_nm
        self.pulse_width_ps: float = 10.0  # 10 ps pulse

    def to_dict(self) -> Dict[str, Any]:
        return {
            "assertion": self.assertion_id,
            "time_ps": round(self.timestamp_ps, 2),
            "wavelength_nm": self.wavelength_nm,
            "pulse_width_ps": self.pulse_width_ps,
        }


class PhotonicSpikingSigma:
    """Neuromorphic + photonic: spikes as light pulses."""

    def __init__(self) -> None:
        self.spike_train: List[PhotonicSpike] = []
        self.measurements = 0

    def encode_state(self, state: int, base_time_ps: float = 0.0) -> Dict[str, Any]:
        """Encode kernel state as photonic spike train."""
        self.measurements += 1
        t0 = time.perf_counter()
        golden = (1 << N_ASSERTIONS) - 1
        violations = state ^ golden
        spikes = []

        for i in range(N_ASSERTIONS):
            if (violations >> i) & 1:
                # Failed assertion → optical spike at its wavelength channel
                spike = PhotonicSpike(
                    assertion_id=i,
                    timestamp_ps=base_time_ps + i * 0.5,  # 0.5 ps between channels
                    wavelength_nm=1530.0 + i * 0.8,  # WDM grid
                )
                spikes.append(spike)
                self.spike_train.append(spike)

        sigma = len(spikes)
        elapsed_us = (time.perf_counter() - t0) * 1e6

        return {
            "state": f"0x{state:05X}",
            "sigma": sigma,
            "spikes": [s.to_dict() for s in spikes],
            "spike_rate_GHz": round(sigma / max(N_ASSERTIONS * 0.5, 1) * 1000, 1) if sigma > 0 else 0.0,
            "total_window_ps": N_ASSERTIONS * 0.5,
            "elapsed_us": round(elapsed_us, 2),
        }

    def firing_rate(self) -> Dict[str, Any]:
        """Population firing rate = σ."""
        if not self.spike_train:
            return {"firing_rate": 0, "sigma_equivalent": 0}

        # Count spikes per assertion
        by_assertion = {}
        for s in self.spike_train:
            by_assertion.setdefault(s.assertion_id, 0)
            by_assertion[s.assertion_id] += 1

        return {
            "total_spikes": len(self.spike_train),
            "assertions_spiking": len(by_assertion),
            "spike_counts": by_assertion,
            "avg_rate": round(len(self.spike_train) / max(self.measurements, 1), 2),
        }

    def convergence(self) -> Dict[str, Any]:
        """Where biology meets photonics."""
        return {
            "biological": "LIF neurons, membrane potential, spike threshold",
            "photonic": "Optical pulse, phase threshold, interference",
            "combined": "Photonic spiking: biological computation at light speed",
            "l53": "Spiking Genesis — assertion failures as neural spikes",
            "l122": "Photonic kernel — assertions as interferometers",
            "l126": "This — spikes as optical pulses. Brain + laser.",
        }

    @property
    def stats(self) -> Dict[str, Any]:
        return {"measurements": self.measurements, "total_spikes": len(self.spike_train)}


def main() -> None:
    import argparse
    ap = argparse.ArgumentParser(description="Photonic Spiking σ")
    ap.add_argument("--demo", action="store_true")
    args = ap.parse_args()

    ps = PhotonicSpikingSigma()
    if args.demo:
        r = ps.encode_state(0x3FFFF)
        print(f"Golden: σ={r['sigma']}, spikes={len(r['spikes'])}")
        r2 = ps.encode_state(0x3FFFF ^ 0x1F)
        print(f"5 violations: σ={r2['sigma']}, spikes={len(r2['spikes'])}")
        fr = ps.firing_rate()
        print(f"Total spikes: {fr['total_spikes']}")
        c = ps.convergence()
        print(f"\n{c['combined']}")
        print("\nBiology + photonics. Spikes at light speed. 1 = 1.")
        return
    print("Photonic Spiking σ. 1 = 1.")


if __name__ == "__main__":
    main()
