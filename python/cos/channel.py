# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Shannon-style **channel picture** for σ (lab metaphor).

Treat the gate’s coherence readout as a **noise level** on a **binary symmetric–channel**
story: :math:`C \\approx 1 - H(\\sigma)` when :math:`\\sigma \\in (0,1)` is the crossover
probability, with **noiseless** (:math:`\\sigma \\rightarrow 0`) mapped to **capacity 1**
and **useless** (:math:`\\sigma \\ge 1` on this scale) to **capacity 0**.

This is a **pedagogical isomorphism** — not a claim that :class:`~cos.sigma_gate.SigmaGate`
is a Shannon-optimal physical channel, that the gate achieves channel capacity, or that
cognitive errors satisfy the noisy-channel coding theorem literally. **Not AGI achieved.**
See ``docs/CLAIM_DISCIPLINE.md``.
"""
from __future__ import annotations

import math
from typing import Any, Dict, List, Sequence, Tuple

from cos.sigma_gate import SigmaGate

__all__ = ["SigmaChannel"]


def _verdict_str(verdict: Any) -> str:
    return str(getattr(verdict, "name", verdict))


def _binary_entropy(p: float) -> float:
    if p <= 0.0 or p >= 1.0:
        return 0.0
    return float(-p * math.log2(p) - (1.0 - p) * math.log2(1.0 - p))


class SigmaChannel:
    """Toy Shannon channel metrics layered on :class:`SigmaGate` scores."""

    def __init__(self, gate: Any = None) -> None:
        self.gate = gate or SigmaGate()
        self.transmissions: List[Dict[str, Any]] = []

    def capacity(self, sigma: float) -> float:
        """BSC-style capacity: :math:`C \\approx 1 - H(\\sigma)` on :math:`\\sigma \\in (0,1)`.

        - :math:`\\sigma \\le 0` → **1.0** (noiseless limit).
        - :math:`\\sigma \\ge 1` → **0.0** (no reliable bits on this normalization).
        """
        s = float(sigma)
        if s <= 0.0:
            return 1.0
        if s >= 1.0:
            return 0.0
        h = _binary_entropy(s)
        return round(max(0.0, 1.0 - h), 4)

    def transmit(self, message: str, context: str = "") -> Dict[str, Any]:
        """Score ``message`` against ``context``; attach capacity and toy bit bookkeeping."""
        ctx = (context or "").strip() or "transmit"
        sigma, verdict = self.gate.score(ctx, str(message))
        s = float(sigma)
        c = self.capacity(s)
        ntok = max(1, len(str(message).split()))

        result: Dict[str, Any] = {
            "message": str(message)[:100],
            "σ": round(s, 4),
            "sigma": round(s, 4),
            "capacity": c,
            "verdict": _verdict_str(verdict),
            "bits_preserved": round(c * ntok, 1),
            "bits_lost": round((1.0 - c) * ntok, 1),
        }
        self.transmissions.append(result)
        return result

    def mutual_information(
        self,
        pairs: Sequence[Tuple[str, str]],
    ) -> float:
        """Aggregate gate σ over ``(declared, realized)`` pairs; return ``capacity(mean σ)``.

        This is **not** a plug-in estimator of :math:`I(X;Y)` from samples — it reuses the
        gate as a fixed scoring channel for tracing only.
        """
        if not pairs:
            return 0.0
        sigmas: List[float] = []
        for declared, realized in pairs:
            si, _v = self.gate.score(str(declared), str(realized))
            sigmas.append(float(si))
        avg_s = sum(sigmas) / len(sigmas)
        return float(self.capacity(avg_s))

    def rate_distortion(
        self,
        messages: Sequence[str],
        target_sigma: float,
    ) -> Dict[str, Any]:
        """Toy rate–distortion bookkeeping: higher σ vs target ⇒ extra fractional bits."""
        tgt = float(target_sigma)
        total_bits = 0.0
        for msg in messages:
            s, _v = self.gate.score("encode", str(msg))
            sf = float(s)
            ntok = max(1, len(str(msg).split()))
            if sf > tgt:
                extra = math.log2(max(sf / max(tgt, 1e-9), 1.01))
                total_bits += ntok + extra
            else:
                total_bits += ntok
        n = len(messages)
        return {
            "total_bits": round(total_bits, 1),
            "target_σ": round(tgt, 4),
            "messages": n,
            "bits_per_message": round(total_bits / max(n, 1), 1),
        }

    def shannon_limit(self) -> Dict[str, Any]:
        """Summarize average noise and mean capacity over recorded :meth:`transmit` calls."""
        if not self.transmissions:
            return {"limit": "unknown", "reason": "no data"}
        avg_s = sum(float(t["σ"]) for t in self.transmissions) / len(self.transmissions)
        c = self.capacity(avg_s)
        eff = sum(float(t["capacity"]) for t in self.transmissions) / len(self.transmissions)
        return {
            "avg_σ": round(avg_s, 4),
            "channel_capacity": c,
            "efficiency": round(eff, 4),
            "shannon_says": (
                f"This channel can transmit at most {c:.1%} of input information. "
                f"σ={avg_s:.3f} is the noise floor."
            ),
        }
