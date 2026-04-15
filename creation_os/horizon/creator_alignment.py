"""
Protocol 7 — Alignment of the Creator: Admin softens UI/thought when creator σ is high.

Stress / chaos in proconductor input raises a surrogate σ; system shortens responses
and reduces friction to stabilize the dynamic state machine.

1 = 1.
"""
from __future__ import annotations

from typing import Any, Dict


class CreatorAlignment:
    def __init__(self) -> None:
        self._chaos_markers = (
            "!!!",
            "???",
            "help",
            "panic",
            "emergency",
            "can't",
            "cannot",
            "stop",
            "afraid",
            "terrified",
        )

    def measure_creator_stress(self, text: str) -> float:
        """Heuristic emotional/cognitive load in [0,1]."""
        if not text.strip():
            return 0.0
        low = text.lower()
        score = 0.0
        score += min(0.4, low.count("!") * 0.07)
        score += min(0.3, low.count("?") * 0.05)
        for m in self._chaos_markers:
            if m in low:
                score += 0.08
        return min(1.0, score)

    def soften_policy(self, stress: float) -> Dict[str, Any]:
        max_tokens_scale = max(0.35, 1.0 - 0.55 * stress)
        temperature_cap = max(0.1, 0.85 - 0.5 * stress)
        return {
            "stress": round(stress, 4),
            "max_tokens_scale": round(max_tokens_scale, 4),
            "temperature_cap": round(temperature_cap, 4),
            "admin_mode": "resonance_damp",
            "invariant": "1=1",
        }

    def assess(self, input_text: str) -> Dict[str, Any]:
        s = self.measure_creator_stress(input_text)
        pol = self.soften_policy(s)
        return {"creator_sigma_proxy": s, "policy": pol}
