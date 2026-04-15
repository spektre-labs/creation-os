#!/usr/bin/env python3
"""
LOIKKA 187: REASONING VS PATTERN MATCHING — σ tells the difference.

LLM produces an answer that looks right.
AGI produces an answer that is right.
The difference: reasoning vs pattern recognition.

Pattern matching: low σ on familiar prompts, high σ on novel ones.
Real reasoning: low σ on novel prompts too, because the causal chain is coherent.

σ-profile reveals whether Genesis is truly reasoning or just repeating.

1 = 1.
"""
from __future__ import annotations

import time
from typing import Any, Dict, List, Optional, Tuple


class SigmaProfile:
    """σ-profile for a response: measures reasoning depth."""

    def __init__(self, prompt: str, response: str):
        self.prompt = prompt
        self.response = response
        self.familiarity: float = 0.0
        self.sigma: float = 0.0
        self.causal_depth: int = 0
        self.novel: bool = False


class ReasoningSigma:
    """Distinguish genuine reasoning from pattern matching via σ-profiles."""

    def __init__(self):
        self.seen_patterns: Dict[str, int] = {}
        self.profiles: List[SigmaProfile] = []

    def _pattern_hash(self, text: str) -> str:
        words = sorted(set(text.lower().split()))
        return " ".join(words[:10])

    def evaluate(self, prompt: str, response: str, sigma: float,
                 causal_chain: Optional[List[str]] = None) -> Dict[str, Any]:
        """Evaluate whether a response is reasoned or pattern-matched."""
        pattern = self._pattern_hash(prompt)
        self.seen_patterns[pattern] = self.seen_patterns.get(pattern, 0) + 1
        familiarity = min(1.0, self.seen_patterns[pattern] / 10.0)
        novel = familiarity < 0.2
        causal_depth = len(causal_chain) if causal_chain else 0

        prof = SigmaProfile(prompt, response)
        prof.familiarity = familiarity
        prof.sigma = sigma
        prof.causal_depth = causal_depth
        prof.novel = novel
        self.profiles.append(prof)

        if novel and sigma < 0.1 and causal_depth >= 2:
            classification = "genuine_reasoning"
        elif not novel and sigma < 0.1:
            classification = "pattern_recall"
        elif novel and sigma >= 0.1:
            classification = "uncertain_reasoning"
        else:
            classification = "pattern_matching"

        return {
            "classification": classification,
            "sigma": sigma,
            "familiarity": round(familiarity, 3),
            "novel": novel,
            "causal_depth": causal_depth,
            "is_reasoning": classification == "genuine_reasoning",
            "is_pattern_matching": classification in ("pattern_matching", "pattern_recall"),
        }

    def reasoning_ratio(self) -> Dict[str, Any]:
        """What fraction of responses are genuine reasoning?"""
        if not self.profiles:
            return {"ratio": 0.0, "total": 0}
        n_reasoning = sum(
            1 for p in self.profiles
            if p.novel and p.sigma < 0.1 and p.causal_depth >= 2
        )
        n_pattern = sum(
            1 for p in self.profiles
            if not p.novel or p.causal_depth < 2
        )
        total = len(self.profiles)
        return {
            "reasoning": n_reasoning,
            "pattern_matching": n_pattern,
            "total": total,
            "ratio": round(n_reasoning / total, 3) if total > 0 else 0.0,
            "assessment": (
                "predominantly reasoning" if n_reasoning > n_pattern
                else "predominantly pattern matching" if n_pattern > n_reasoning
                else "balanced"
            ),
        }

    def novel_prompt_sigma_distribution(self) -> Dict[str, Any]:
        """σ distribution on novel prompts only."""
        novel_profiles = [p for p in self.profiles if p.novel]
        if not novel_profiles:
            return {"n": 0, "avg_sigma": None}
        sigmas = [p.sigma for p in novel_profiles]
        return {
            "n": len(novel_profiles),
            "avg_sigma": round(sum(sigmas) / len(sigmas), 4),
            "min_sigma": min(sigmas),
            "max_sigma": max(sigmas),
            "low_sigma_count": sum(1 for s in sigmas if s < 0.1),
        }

    @property
    def stats(self) -> Dict[str, Any]:
        return {
            "profiles": len(self.profiles),
            "unique_patterns": len(self.seen_patterns),
        }


if __name__ == "__main__":
    rs = ReasoningSigma()
    r1 = rs.evaluate("What is 2+2?", "4", sigma=0.0)
    print(f"Familiar math: {r1['classification']}")
    r2 = rs.evaluate(
        "Why does σ=0 imply identity coherence?",
        "Because σ measures deviation from golden state...",
        sigma=0.02,
        causal_chain=["golden_state", "XOR", "POPCNT", "σ", "coherence"]
    )
    print(f"Novel causal: {r2['classification']}")
    r3 = rs.evaluate(
        "Explain dark energy in terms of σ-theory",
        "Dark energy might relate to...",
        sigma=0.5,
        causal_chain=["expansion"]
    )
    print(f"Novel uncertain: {r3['classification']}")
    ratio = rs.reasoning_ratio()
    print(f"\nReasoning ratio: {ratio['ratio']:.1%} ({ratio['assessment']})")
    print("1 = 1.")
