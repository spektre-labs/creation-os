# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
σ-spike — event-driven token / phase processing (Python lab).

Compute τ only when the hidden representation moves more than ``threshold`` (cosine distance).
Does **not** touch ``sigma_gate.h``; aligns with ``src/sigma/sigma_spike.c`` intent.

Energy / µJ numbers in CLI are **unitless lab ratios** unless bound to power traces;
see ``docs/CLAIM_DISCIPLINE.md``.
"""
from __future__ import annotations

import hashlib
import math
from typing import Any, Dict, List, Optional, Protocol, Sequence


class SigmaGateHidden(Protocol):
    def compute_sigma_from_hidden(self, hidden: Sequence[float]) -> float:
        ...


class _DefaultLabGate:
    """Deterministic σ from L1 norm of hidden (harness only)."""

    def compute_sigma_from_hidden(self, hidden: Sequence[float]) -> float:
        if not hidden:
            return 0.0
        s = sum(abs(float(x)) for x in hidden)
        return min(1.0, s / (len(hidden) * 4.0))


class SigmaSpike:
    """Skip σ when cosine distance from last hidden < threshold."""

    def __init__(self, gate: Optional[SigmaGateHidden] = None, *, threshold: float = 0.05) -> None:
        self.gate = gate or _DefaultLabGate()
        self.threshold = float(threshold)
        self.last_hidden: Optional[List[float]] = None
        self.stats: Dict[str, int] = {"fired": 0, "suppressed": 0}

    @staticmethod
    def cosine_distance(a: Sequence[float], b: Sequence[float]) -> float:
        if not a or not b:
            return 1.0
        n = min(len(a), len(b))
        dot = 0.0
        na = 0.0
        nb = 0.0
        for i in range(n):
            x = float(a[i])
            y = float(b[i])
            dot += x * y
            na += x * x
            nb += y * y
        denom = math.sqrt(na) * math.sqrt(nb) + 1e-12
        sim = dot / denom
        return max(0.0, 1.0 - sim)

    def process_token(self, hidden_state: Sequence[float]) -> Optional[float]:
        h = [float(x) for x in hidden_state]
        if self.last_hidden is not None:
            delta = self.cosine_distance(h, self.last_hidden)
            if delta < self.threshold:
                self.stats["suppressed"] += 1
                return None
        self.last_hidden = h
        self.stats["fired"] += 1
        sigma = self.gate.compute_sigma_from_hidden(h)
        return float(sigma)

    def sparsity(self) -> float:
        total = self.stats["fired"] + self.stats["suppressed"]
        if total == 0:
            return 0.0
        return self.stats["suppressed"] / float(total)


def hidden_from_token(tok: str, dim: int = 8) -> List[float]:
    raw = hashlib.sha256(tok.encode("utf-8")).digest()
    vec: List[float] = []
    for i in range(max(1, int(dim))):
        b = raw[i % len(raw)]
        vec.append((b / 255.0) - 0.5)
    return vec


def prompt_drive_q15(text: str, *, spread: float = 1.0) -> int:
    """Map free-text to an integer drive for LIF inference (lab deterministic hash)."""
    h = hashlib.sha256(str(text).encode("utf-8")).digest()
    v = int.from_bytes(h[:2], "big")
    base = 80 + (v % 400)
    return max(30, int(base * float(spread)))


COS_Q16_I = 65536
_K_CRIT_I = int(0.127 * COS_Q16_I)


def _gate_update_int(st: Dict[str, int], new_sig: int, k_raw: int) -> None:
    st["d_sigma"] = new_sig - st["sigma"]
    st["sigma"] = new_sig
    st["k_eff"] = int((COS_Q16_I - st["sigma"]) * k_raw / COS_Q16_I)


def _membrane_to_sigma_q16(membrane: int, threshold: int) -> int:
    thr = threshold if threshold > 0 else 1
    mcap = membrane
    if mcap > thr * 3:
        mcap = thr * 3
    num = (mcap * COS_Q16_I) // (thr * 4)
    return min(COS_Q16_I, max(0, num))


class SigmaSpikeNetwork:
    """Leaky integrate-and-fire stack with Q16 σ-gate suppression (ABSTAIN via k_eff)."""

    def __init__(
        self,
        n_layers: int,
        *,
        threshold: int = 100,
        leak: int = 5,
        refractory: int = 3,
    ) -> None:
        self.layers: List[Dict[str, Any]] = []
        for _ in range(max(1, int(n_layers))):
            self.layers.append(
                {
                    "membrane": 0,
                    "threshold": int(threshold),
                    "leak": int(leak),
                    "refractory": int(refractory),
                    "refrac_counter": 0,
                    "spikes_fired": 0,
                    "spikes_suppressed": 0,
                    "gate": {"sigma": 0, "d_sigma": 0, "k_eff": COS_Q16_I},
                }
            )

    def lif_idle(self, neuron: Dict[str, Any]) -> None:
        if neuron["refrac_counter"] > 0:
            neuron["refrac_counter"] -= 1
        neuron["spikes_suppressed"] += 1

    def lif_step(self, neuron: Dict[str, Any], input_val: int, k_raw: int) -> int:
        if neuron["refrac_counter"] > 0:
            neuron["refrac_counter"] -= 1
            neuron["spikes_suppressed"] += 1
            return 0

        neuron["membrane"] += int(input_val)
        neuron["membrane"] -= neuron["leak"]
        neuron["membrane"] = max(0, neuron["membrane"])

        if neuron["membrane"] <= neuron["threshold"]:
            neuron["spikes_suppressed"] += 1
            return 0

        sig = _membrane_to_sigma_q16(neuron["membrane"], neuron["threshold"])
        g = neuron["gate"]
        _gate_update_int(g, sig, k_raw)

        if g["k_eff"] < _K_CRIT_I:
            half = max(0, neuron["threshold"] // 2)
            neuron["membrane"] = half
            neuron["spikes_suppressed"] += 1
            return 0

        neuron["membrane"] = 0
        neuron["refrac_counter"] = neuron["refractory"]
        neuron["spikes_fired"] += 1
        return 1

    def forward(self, input0: int, *, k_raw: int = int(0.9 * COS_Q16_I)) -> Dict[str, Any]:
        spikes: List[int] = []
        carry = int(input0)
        k = int(k_raw)
        dec = 85
        for i, layer in enumerate(self.layers):
            sp = self.lif_step(layer, carry, k)
            spikes.append(sp)
            if sp == 0:
                for j in range(i + 1, len(self.layers)):
                    self.lif_idle(self.layers[j])
                break
            thr = layer["threshold"]
            carry = max(thr // 4, (carry * dec) // 100)

        active = sum(1 for s in spikes if s > 0)
        tot = len(self.layers)
        return {
            "spikes": spikes,
            "active_layers": active,
            "total_layers": tot,
            "sparsity": 1.0 - (active / float(tot)) if tot else 0.0,
        }

    def forward_dense_baseline(
        self,
        input0: int,
        *,
        k_raw: int = int(0.9 * COS_Q16_I),
    ) -> Dict[str, Any]:
        """Process every layer (no early exit) — lab cost proxy for dense depth-wise eval."""
        spikes: List[int] = []
        nxt = int(input0)
        k = int(k_raw)
        for layer in self.layers:
            sp = self.lif_step(layer, nxt, k)
            spikes.append(sp)
            thr = layer["threshold"]
            leak = layer["leak"]
            nxt = thr + leak + max(1, thr // 2)
        active = sum(1 for s in spikes if s > 0)
        tot = len(self.layers)
        return {
            "spikes": spikes,
            "active_layers": active,
            "total_layers": tot,
            "sparsity": 1.0 - (active / float(tot)) if tot else 0.0,
        }

    def energy_report(self) -> Dict[str, Any]:
        total_fired = sum(int(ly["spikes_fired"]) for ly in self.layers)
        total_suppressed = sum(int(ly["spikes_suppressed"]) for ly in self.layers)
        total = total_fired + total_suppressed
        sparse = (total_suppressed / float(total)) if total else 0.0
        denom = max(0.01, 1.0 - sparse)
        return {
            "spikes_fired": total_fired,
            "spikes_suppressed": total_suppressed,
            "sparsity": round(sparse, 4),
            "energy_savings_x": round(1.0 / denom, 2),
        }


def benchmark_lif_vs_dense_wallclock_lab(n: int = 1000) -> Dict[str, Any]:
    """Wall-clock lab ratio only — not µJ-instrumented silicon (see CLAIM_DISCIPLINE)."""
    import time

    n_it = max(10, int(n))
    drive = 220
    t0 = time.perf_counter()
    for _ in range(n_it):
        net = SigmaSpikeNetwork(14)
        net.forward(drive)
    t1 = time.perf_counter()
    spike_ms = (t1 - t0) * 1000.0 / float(n_it)

    t0 = time.perf_counter()
    for _ in range(n_it):
        net_d = SigmaSpikeNetwork(14)
        net_d.forward_dense_baseline(drive)
    t1 = time.perf_counter()
    dense_ms = (t1 - t0) * 1000.0 / float(n_it)

    return {
        "n": n_it,
        "spike_ms_avg": round(spike_ms, 4),
        "dense_ms_avg": round(dense_ms, 4),
        "speedup_x": round(dense_ms / max(spike_ms, 1e-9), 2),
        "note": "Python wall-clock lab only — not RISC-V or Loihi measurements.",
    }


class OmegaSpikeLab:
    """14 lanes × ``SigmaSpike`` (Python mirror of Ω phase spike bundle)."""

    def __init__(self, *, threshold: float = 0.05) -> None:
        self.lanes = [SigmaSpike(threshold=threshold) for _ in range(14)]

    def step(self, phase_hiddens: Sequence[Sequence[float]]) -> Dict[str, Any]:
        fired = 0
        sigmas: List[Optional[float]] = []
        for i, lane in enumerate(self.lanes):
            h = list(phase_hiddens[i]) if i < len(phase_hiddens) else [0.0]
            o = lane.process_token(h)
            sigmas.append(o)
            if o is not None:
                fired += 1
        sup = sum(lane.stats["suppressed"] for lane in self.lanes)
        tot = sum(lane.stats["fired"] + lane.stats["suppressed"] for lane in self.lanes)
        sp = (sup / float(tot)) if tot else 0.0
        return {
            "phases_fired_this_step": fired,
            "lane_events": tot,
            "sparsity": sp,
            "sigmas": sigmas,
        }


def benchmark_spike_vs_continuous_mj_per_token() -> Dict[str, Any]:
    """
    Toy energy model: continuous pays full cost per token; spike pays only on fire.
    Values are **relative** lab placeholders (not measured µJ).
    """
    base_mj = 0.12
    spike_mj = 0.02
    return {
        "continuous_mj_per_token": base_mj,
        "spike_mj_per_token_lab_model": spike_mj,
        "ratio_continuous_over_spike": round(base_mj / spike_mj, 2),
        "note": "Lab model only — not instrumented silicon; see docs/CLAIM_DISCIPLINE.md",
    }


__all__ = [
    "OmegaSpikeLab",
    "SigmaSpike",
    "SigmaSpikeNetwork",
    "benchmark_spike_vs_continuous_mj_per_token",
    "hidden_from_token",
    "prompt_drive_q15",
    "benchmark_lif_vs_dense_wallclock_lab",
]
