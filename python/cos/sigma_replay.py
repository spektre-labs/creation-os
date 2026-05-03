# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-replay: experience replay filtered by gate verdict (ACCEPT-only intake)."""
from __future__ import annotations

import random
from typing import Any, Dict, List, Optional, Sequence, Tuple

from .sigma_gate_core import Verdict


class SigmaReplayBuffer:
    """Ring of ACCEPT experiences; eviction drops highest-σ rows when over capacity."""

    def __init__(
        self,
        max_size: int = 10_000,
        gate: Optional[Any] = None,
        *,
        rng: Optional[random.Random] = None,
    ) -> None:
        self.max_size = int(max_size)
        self.buffer: List[Dict[str, Any]] = []
        self.gate = gate
        self._rng = rng or random.Random()

    def add(self, prompt: str, response: str, sigma: float, verdict: Verdict) -> bool:
        """Append one row if ``verdict == ACCEPT``; evict highest σ when full."""
        if verdict != Verdict.ACCEPT:
            return False
        self.buffer.append(
            {
                "prompt": str(prompt),
                "response": str(response),
                "sigma": float(sigma),
                "age": 0,
            }
        )
        if len(self.buffer) > self.max_size:
            self.buffer.sort(key=lambda x: float(x["sigma"]))
            # retain lowest-sigma (most “trusted”) experiences
            self.buffer = self.buffer[: self.max_size]
        for i, row in enumerate(self.buffer):
            row["age"] = int(i)
        return True

    def sample(self, n: int = 32) -> List[Dict[str, Any]]:
        """Weighted sampling favouring low σ (weight ∝ 1/(σ+ε))."""
        if not self.buffer or n <= 0:
            return []
        n = min(int(n), len(self.buffer))
        weights = [1.0 / (float(x["sigma"]) + 0.01) for x in self.buffer]
        total = sum(weights)
        if total <= 0:
            return self._rng.sample(self.buffer, k=n)
        probs = [w / total for w in weights]
        return self._rng.choices(self.buffer, weights=probs, k=n)

    def mix_with_new(
        self,
        new_data: Sequence[Tuple[str, str]],
        *,
        replay_ratio: float = 0.3,
    ) -> List[Tuple[str, str]]:
        """Interleave replay draws with ``new_data`` and shuffle."""
        nd = list(new_data)
        if not nd or not self.buffer:
            mixed = list(nd)
            self._rng.shuffle(mixed)
            return mixed
        n_replay = int(len(nd) * float(replay_ratio))
        replay = self.sample(max(1, n_replay)) if n_replay > 0 else []
        mixed = list(nd) + [(str(r["prompt"]), str(r["response"])) for r in replay]
        self._rng.shuffle(mixed)
        return mixed

    def stats(self) -> Dict[str, Any]:
        """Summary counters for `cos learn --replay --stats`."""
        if not self.buffer:
            return {
                "n": 0,
                "avg_sigma": 0.0,
                "max_age": 0,
                "min_sigma": None,
                "max_sigma": None,
            }
        sigmas = [float(x["sigma"]) for x in self.buffer]
        ages = [int(x.get("age", 0)) for x in self.buffer]
        return {
            "n": len(self.buffer),
            "avg_sigma": sum(sigmas) / float(len(sigmas)),
            "max_age": max(ages) if ages else 0,
            "min_sigma": min(sigmas),
            "max_sigma": max(sigmas),
        }


__all__ = ["SigmaReplayBuffer"]
