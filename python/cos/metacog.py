# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-metacognition: separate roughly **epistemic** vs **aleatoric** uncertainty for routing.

Epistemic signals (model-side): σ-gate, mean log-prob, hedging phrasing, multi-sample
consistency when provided. Aleatoric signals (problem-side): terse or vague prompts,
underspecified phrasing, optional ``ambiguity_score`` from a parser.

This is a lightweight heuristic layer — not a calibrated probabilistic model. For
claim hygiene on benchmarks see ``docs/CLAIM_DISCIPLINE.md``.
"""
from __future__ import annotations

import math
import time
from typing import Any, Dict, List, Optional


class UncertaintyType:
    EPISTEMIC = "epistemic"
    ALEATORIC = "aleatoric"
    LOW = "low"
    MIXED = "mixed"


class SigmaMetacog:
    """Heuristic metacognitive routing on top of σ-gate and optional token-level signals."""

    def __init__(self, epistemic_threshold: float = 0.5, aleatoric_threshold: float = 0.5) -> None:
        self.epistemic_threshold = float(epistemic_threshold)
        self.aleatoric_threshold = float(aleatoric_threshold)
        self.history: List[Dict[str, Any]] = []

    def assess(
        self,
        prompt: str,
        response: str,
        sigma: float,
        signals: Optional[Dict[str, Any]] = None,
    ) -> Dict[str, Any]:
        signals = dict(signals or {})

        epistemic = self._estimate_epistemic(prompt, response, sigma, signals)
        aleatoric = self._estimate_aleatoric(prompt, signals)

        total = math.sqrt(epistemic**2 + aleatoric**2)
        total = min(1.0, total)

        if epistemic < self.epistemic_threshold and aleatoric < self.aleatoric_threshold:
            utype = UncertaintyType.LOW
        elif epistemic >= self.epistemic_threshold and aleatoric < self.aleatoric_threshold:
            utype = UncertaintyType.EPISTEMIC
        elif aleatoric >= self.aleatoric_threshold and epistemic < self.epistemic_threshold:
            utype = UncertaintyType.ALEATORIC
        else:
            utype = UncertaintyType.MIXED

        action = self._recommend_action(utype, sigma, epistemic, aleatoric)

        assessment = {
            "uncertainty_type": utype,
            "epistemic_sigma": round(epistemic, 4),
            "aleatoric_sigma": round(aleatoric, 4),
            "total_sigma": round(total, 4),
            "gate_sigma": round(float(sigma), 4),
            "action": action,
            "timestamp": time.time(),
        }
        self.history.append(assessment)
        return assessment

    def _estimate_epistemic(
        self,
        prompt: str,
        response: str,
        sigma: float,
        signals: Dict[str, Any],
    ) -> float:
        score = 0.0
        n_signals = 0

        score += float(sigma)
        n_signals += 1

        if "logprob_mean" in signals:
            lp = float(signals["logprob_mean"])
            lp_score = max(0.0, min(1.0, -lp / 5.0))
            score += lp_score
            n_signals += 1

        hedging_words = (
            "maybe",
            "perhaps",
            "possibly",
            "might",
            "could be",
            "i think",
            "i believe",
            "not sure",
            "uncertain",
            "it depends",
            "generally",
            "typically",
            "arguably",
        )
        response_lower = response.lower()
        hedge_count = sum(1 for h in hedging_words if h in response_lower)
        hedge_score = min(1.0, hedge_count * 0.2)
        score += hedge_score
        n_signals += 1

        if "consistency_score" in signals:
            cs = float(signals["consistency_score"])
            score += 1.0 - cs
            n_signals += 1

        return score / max(n_signals, 1)

    def _estimate_aleatoric(self, prompt: str, signals: Dict[str, Any]) -> float:
        score = 0.0
        n_signals = 0

        words = prompt.split()
        if len(words) < 3:
            score += 0.7
        elif len(words) <= 4:
            # Three- to four-word prompts are often underspecified (e.g. "What about it?").
            score += 0.7
        elif len(words) < 6:
            score += 0.3
        n_signals += 1

        ambiguous_patterns = (
            "what about",
            "how about",
            "tell me about",
            "what do you think",
            "is it",
            "can you",
        )
        prompt_lower = prompt.lower()
        ambig_count = sum(1 for a in ambiguous_patterns if a in prompt_lower)
        score += min(1.0, ambig_count * 0.3)
        n_signals += 1

        if "ambiguity_score" in signals:
            score += float(signals["ambiguity_score"])
            n_signals += 1

        return score / max(n_signals, 1)

    def _recommend_action(
        self,
        utype: str,
        sigma: float,
        epistemic: float,
        _aleatoric: float,
    ) -> str:
        if utype == UncertaintyType.LOW:
            return "respond"

        if utype == UncertaintyType.EPISTEMIC:
            if epistemic > 0.8:
                return "abstain"
            return "retrieve"

        if utype == UncertaintyType.ALEATORIC:
            return "clarify"

        if utype == UncertaintyType.MIXED:
            if sigma > 0.8:
                return "abstain"
            return "clarify_and_retrieve"

        return "respond"

    def reflect(self, assessment: Dict[str, Any], outcome: Dict[str, Any]) -> Dict[str, Any]:
        """Light feedback: nudge epistemic threshold from coarse outcomes (lab hook)."""

        entry = {
            "assessment": assessment,
            "outcome": outcome,
            "timestamp": time.time(),
        }

        if outcome.get("correct") is False:
            if assessment.get("uncertainty_type") == UncertaintyType.LOW:
                self.epistemic_threshold = max(0.2, self.epistemic_threshold - 0.05)

        if outcome.get("correct") is True:
            if assessment.get("action") == "abstain":
                self.epistemic_threshold = min(0.9, self.epistemic_threshold + 0.02)

        return entry

    def get_stats(self) -> Dict[str, Any]:
        if not self.history:
            return {"total": 0}

        types = [a["uncertainty_type"] for a in self.history]
        actions = [a["action"] for a in self.history]

        return {
            "total": len(self.history),
            "epistemic": types.count(UncertaintyType.EPISTEMIC),
            "aleatoric": types.count(UncertaintyType.ALEATORIC),
            "low": types.count(UncertaintyType.LOW),
            "mixed": types.count(UncertaintyType.MIXED),
            "respond": actions.count("respond"),
            "retrieve": actions.count("retrieve"),
            "clarify": actions.count("clarify"),
            "abstain": actions.count("abstain"),
            "clarify_and_retrieve": actions.count("clarify_and_retrieve"),
            "avg_epistemic": sum(a["epistemic_sigma"] for a in self.history) / len(self.history),
            "avg_aleatoric": sum(a["aleatoric_sigma"] for a in self.history) / len(self.history),
            "current_threshold": self.epistemic_threshold,
        }


__all__ = ["SigmaMetacog", "UncertaintyType"]
