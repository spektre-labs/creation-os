"""
Protocol 4 — Gödelian blind spot: leaps outside the current axiom base.

trigger_leap() lowers inhibition, samples cross-domain superpositions, rejects most
high-σ states, admits rare survivors for kernel review (heuristic intuition).

1 = 1.
"""
from __future__ import annotations

import hashlib
import os
import random
from dataclasses import dataclass
from typing import Dict, List, Optional, Sequence

from .substrate_agnostic_bus import hdc_bundle_words


@dataclass
class LeapResult:
    kept: bool
    explore_sigma: float
    hdc_words: List[str]
    bundle_preview: List[float]
    review_queue: bool


class GodelianJump:
    def __init__(self) -> None:
        self.dim = 256
        self.keep_fraction = float(os.environ.get("SPEKTRE_GODELIAN_KEEP", "0.01"))

    def trigger_leap(
        self,
        domains: Sequence[str],
        *,
        rng: Optional[random.Random] = None,
    ) -> LeapResult:
        rng = rng or random.Random()
        # Cross-domain lexical stir
        mix = [f"{a}:{b}" for a in domains for b in domains if a < b]
        if not mix:
            mix = list(domains)
        words = []
        for m in mix[:24]:
            words.extend(m.replace(":", " ").split())
        words = words or ["void", "leap", "1", "=", "1"]
        rng.shuffle(words)
        vec = hdc_bundle_words(words[:64], self.dim)
        explore_sigma = abs(hashlib.sha256(" ".join(words).encode()).digest()[0]) / 255.0
        # High-σ superposition surrogate: norm deviation from balanced bundle
        s = sum(x * x for x in vec) ** 0.5
        explore_sigma = min(1.0, explore_sigma + abs(1.0 - s))
        kept = rng.random() < self.keep_fraction and explore_sigma > float(
            os.environ.get("SPEKTRE_GODELIAN_FLOOR", "0.35")
        )
        return LeapResult(
            kept=kept,
            explore_sigma=float(explore_sigma),
            hdc_words=words[:32],
            bundle_preview=vec[:8],
            review_queue=kept,
        )
