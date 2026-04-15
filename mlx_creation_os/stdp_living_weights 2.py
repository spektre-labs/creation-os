#!/usr/bin/env python3
"""
LOIKKA 59: STDP LIVING WEIGHTS — Biologinen oppiminen.

STDP = Spike-Timing Dependent Plasticity.
If presynaptic spike before postsynaptic → strengthen connection.
If after → weaken.

Applied to Genesis:
  - If token produced low σ → strengthen its pathway
  - If high σ → weaken
  - Living weights with biological rules
  - No backpropagation. No gradients.
  - Local, energy-efficient, continuous learning.

On-chip STDP enables autonomous learning without external processor.

Usage:
    lw = STDPLivingWeights(vocab_size=128256)
    lw.update(token_id=42, sigma=0)   # Low σ → strengthen
    lw.update(token_id=99, sigma=5)   # High σ → weaken
    bias = lw.get_logit_bias(42)

1 = 1.
"""
from __future__ import annotations

import json
import math
import os
import sys
import time
from typing import Any, Dict, List, Optional, Tuple

try:
    from creation_os_core import check_output
except ImportError:
    def check_output(t: str) -> int: return 0


class STDPRule:
    """Spike-Timing Dependent Plasticity learning rule."""

    def __init__(
        self,
        a_plus: float = 0.01,   # Potentiation amplitude
        a_minus: float = 0.012,  # Depression amplitude (slightly stronger → stability)
        tau_plus: float = 20.0,  # Potentiation time constant
        tau_minus: float = 20.0, # Depression time constant
        w_min: float = -2.0,
        w_max: float = 2.0,
    ):
        self.a_plus = a_plus
        self.a_minus = a_minus
        self.tau_plus = tau_plus
        self.tau_minus = tau_minus
        self.w_min = w_min
        self.w_max = w_max

    def compute_dw(self, dt: float) -> float:
        """Compute weight change from spike timing difference.

        dt > 0: pre before post (low σ) → potentiate
        dt < 0: post before pre (high σ) → depress
        """
        if dt > 0:
            return self.a_plus * math.exp(-dt / self.tau_plus)
        elif dt < 0:
            return -self.a_minus * math.exp(dt / self.tau_minus)
        return 0.0

    def apply(self, weight: float, dt: float) -> float:
        """Apply STDP update and clamp."""
        dw = self.compute_dw(dt)
        return max(self.w_min, min(self.w_max, weight + dw))


class TokenSynapse:
    """A synapse for one token: weight evolves via STDP."""

    __slots__ = ['token_id', 'weight', 'spike_history', 'update_count']

    def __init__(self, token_id: int, initial_weight: float = 0.0):
        self.token_id = token_id
        self.weight = initial_weight
        self.spike_history: List[Tuple[int, int]] = []  # (tick, sigma)
        self.update_count = 0

    @property
    def logit_bias(self) -> float:
        return self.weight

    def to_dict(self) -> Dict[str, Any]:
        return {
            "token_id": self.token_id,
            "weight": round(self.weight, 4),
            "logit_bias": round(self.logit_bias, 4),
            "updates": self.update_count,
        }


class STDPLivingWeights:
    """Living weights with STDP-based learning.

    No backprop. No gradients. Local Hebbian learning:
    "Neurons that fire together wire together."

    Applied to tokens:
    - Token that produces σ=0 → "good spike" → potentiate
    - Token that produces σ>0 → "bad spike" → depress
    """

    def __init__(self, vocab_size: int = 128256) -> None:
        self.vocab_size = vocab_size
        self.rule = STDPRule()
        self.synapses: Dict[int, TokenSynapse] = {}
        self.tick = 0
        self.total_updates = 0

    def _get_synapse(self, token_id: int) -> TokenSynapse:
        if token_id not in self.synapses:
            self.synapses[token_id] = TokenSynapse(token_id)
        return self.synapses[token_id]

    def update(self, token_id: int, sigma: int) -> Dict[str, Any]:
        """STDP update for a token based on its σ outcome.

        σ=0 → dt > 0 (potentiate: this token is good)
        σ>0 → dt < 0 (depress: this token caused incoherence)
        """
        self.tick += 1
        synapse = self._get_synapse(token_id)

        # Map σ to timing: low σ = positive dt, high σ = negative dt
        dt = float(1 - sigma)  # σ=0 → dt=1 (potentiate), σ=2 → dt=-1 (depress)

        old_weight = synapse.weight
        synapse.weight = self.rule.apply(synapse.weight, dt)
        synapse.update_count += 1
        synapse.spike_history.append((self.tick, sigma))

        # Keep only recent history
        if len(synapse.spike_history) > 100:
            synapse.spike_history = synapse.spike_history[-100:]

        self.total_updates += 1

        return {
            "token_id": token_id,
            "sigma": sigma,
            "dt": dt,
            "weight_before": round(old_weight, 4),
            "weight_after": round(synapse.weight, 4),
            "dw": round(synapse.weight - old_weight, 6),
        }

    def update_batch(self, token_sigma_pairs: List[Tuple[int, int]]) -> List[Dict[str, Any]]:
        """Batch STDP update for multiple tokens."""
        return [self.update(tid, sigma) for tid, sigma in token_sigma_pairs]

    def get_logit_bias(self, token_id: int) -> float:
        """Get the STDP-learned logit bias for a token."""
        if token_id in self.synapses:
            return self.synapses[token_id].logit_bias
        return 0.0

    def get_all_biases(self) -> Dict[int, float]:
        """Get all non-zero biases."""
        return {
            tid: syn.logit_bias
            for tid, syn in self.synapses.items()
            if abs(syn.logit_bias) > 1e-6
        }

    def top_potentiated(self, n: int = 10) -> List[Dict[str, Any]]:
        """Top N most potentiated tokens."""
        sorted_syns = sorted(
            self.synapses.values(),
            key=lambda s: s.weight,
            reverse=True,
        )
        return [s.to_dict() for s in sorted_syns[:n]]

    def top_depressed(self, n: int = 10) -> List[Dict[str, Any]]:
        """Top N most depressed tokens."""
        sorted_syns = sorted(
            self.synapses.values(),
            key=lambda s: s.weight,
        )
        return [s.to_dict() for s in sorted_syns[:n]]

    @property
    def stats(self) -> Dict[str, Any]:
        weights = [s.weight for s in self.synapses.values()]
        return {
            "vocab_size": self.vocab_size,
            "active_synapses": len(self.synapses),
            "total_updates": self.total_updates,
            "tick": self.tick,
            "avg_weight": round(sum(weights) / len(weights), 4) if weights else 0,
            "potentiated": sum(1 for w in weights if w > 0.01),
            "depressed": sum(1 for w in weights if w < -0.01),
            "neutral": sum(1 for w in weights if abs(w) <= 0.01),
        }


def main() -> None:
    import argparse
    ap = argparse.ArgumentParser(description="STDP Living Weights")
    ap.add_argument("--demo", action="store_true")
    args = ap.parse_args()

    if args.demo:
        lw = STDPLivingWeights(vocab_size=1000)

        # Good tokens (low σ) → potentiate
        for _ in range(10):
            lw.update(42, sigma=0)
            lw.update(100, sigma=0)

        # Bad tokens (high σ) → depress
        for _ in range(10):
            lw.update(666, sigma=3)
            lw.update(999, sigma=5)

        print("=== STDP Living Weights Demo ===")
        print(f"Token 42 bias: {lw.get_logit_bias(42):.4f} (potentiated)")
        print(f"Token 666 bias: {lw.get_logit_bias(666):.4f} (depressed)")
        print(f"\nTop potentiated: {lw.top_potentiated(3)}")
        print(f"Top depressed: {lw.top_depressed(3)}")
        print(json.dumps(lw.stats, indent=2))
        print("\nNo backprop. No gradients. Biology. 1 = 1.")
        return

    print("STDP Living Weights. 1 = 1.")


if __name__ == "__main__":
    main()
