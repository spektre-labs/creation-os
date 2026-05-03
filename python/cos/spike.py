# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-spike — leaky integrate-and-fire lab and add-only attention sketch.

Relates **low** gate σ to “fire” metaphors only at the Python/lab level; it does **not**
modify ``sigma_gate.h`` or assert silicon energy numbers. See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

from typing import Any, Dict, Sequence, Tuple

__all__ = ["SigmaSpike"]


class SigmaSpike:
    """LIF step, spike-bag attention (adds only), and rough energy bookkeeping."""

    @staticmethod
    def lif_step(
        membrane: float,
        input_current: float,
        *,
        threshold: float,
        decay: float,
    ) -> Tuple[float, float]:
        """One Euler step; returns ``(spike, new_membrane)`` with reset on fire."""
        m = float(membrane) * float(decay) + float(input_current)
        th = float(threshold)
        spike = 1.0 if m >= th else 0.0
        if spike:
            m = 0.0
        return spike, m

    @staticmethod
    def spike_attention(
        q_spikes: Sequence[float],
        k_spikes: Sequence[float],
        v: Sequence[float],
    ) -> float:
        """Scalar output = Σ (q·k)·v using only adds (treat products as repeated add in lab)."""
        acc = 0.0
        for qi, ki, vi in zip(q_spikes, k_spikes, v):
            q = 1.0 if float(qi) >= 0.5 else 0.0
            k = 1.0 if float(ki) >= 0.5 else 0.0
            acc += (q * k) * float(vi)
        return acc

    @staticmethod
    def convert_gate_to_spike(gate: Any) -> Dict[str, float]:
        """Map ``SigmaGate`` accept threshold to LIF threshold (lab metaphor)."""
        return {
            "threshold": float(getattr(gate, "threshold_accept", 0.3)),
            "decay": 0.9,
        }

    @staticmethod
    def energy_estimate(
        n_spikes: int,
        n_total: int,
        *,
        add_energy: float = 0.1,
        mul_energy: float = 1.0,
    ) -> Dict[str, float]:
        """Unitless upper-bound toy: spike path pays ``add_energy``, dense pays ``mul``."""
        nt = max(int(n_total), 1)
        ratio = max(0.0, min(1.0, int(n_spikes) / nt))
        dense = (1.0 - ratio) * mul_energy
        sparse = ratio * add_energy
        return {
            "spike_ratio": round(ratio, 6),
            "relative_energy": round(dense + sparse, 6),
        }

    @staticmethod
    def spike_train_to_sigma(spike_train: Sequence[float]) -> float:
        """Turn mean spike rate into a σ-like scalar (more spikes → lower σ)."""
        if not spike_train:
            return 1.0
        rate = sum(1.0 for s in spike_train if float(s) >= 0.5) / len(spike_train)
        return max(0.0, min(1.0, 1.0 - rate))
