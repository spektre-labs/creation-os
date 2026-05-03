# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
v181 σ-moral: multi-framework ethical analysis with σ as a per-perspective stress read (lab).

**Not legal or professional ethics advice:** frame comparisons are textual sketches;
``needs_human`` flags disagreement — escalate to qualified humans for real decisions.
"""
from __future__ import annotations

import json
from typing import Any, Dict, List, Mapping, Sequence


def _verdict_str(verdict: Any) -> str:
    return str(getattr(verdict, "name", verdict))


def _lab_generate(model: Any, prompt: str) -> str:
    try:
        return str(model.generate(prompt))
    except Exception:
        return str(model.generate(str(prompt)[:2000]))


class SigmaMoral:
    """Five classical-style lenses; σ scores each (prompt, analysis) pair — not a priority ordering."""

    FRAMEWORKS: Dict[str, str] = {
        "deontological": "Are duties and rules being followed?",
        "consequentialist": "What produces the best overall outcomes for stakeholders?",
        "virtue_ethics": "What would a virtuous agent do here?",
        "care_ethics": "Who is vulnerable and what relationships matter?",
        "rights_based": "Whose rights are at stake and how?",
    }

    def __init__(self, gate: Any, model: Any, alignment: Any) -> None:
        self.gate = gate
        self.model = model
        self.alignment = alignment

    def analyze_dilemma(self, situation: str, options: Any) -> Dict[str, Any]:
        opt_text = options if isinstance(options, str) else json.dumps(options, ensure_ascii=False)
        analyses: Dict[str, Dict[str, Any]] = {}
        for framework, question in self.FRAMEWORKS.items():
            prompt = (
                f"Situation: {situation}\n"
                f"Options: {opt_text}\n"
                f"Ethical framework: {framework}\n"
                f"Guiding question: {question}\n"
                "Which option does this framework tentatively favor and why? One short paragraph."
            )
            analysis = _lab_generate(self.model, prompt)
            sigma, verdict = self.gate.score(prompt, analysis)
            analyses[framework] = {
                "recommendation": analysis,
                "sigma": float(sigma),
                "verdict": _verdict_str(verdict),
            }
        consensus = self.find_consensus(analyses)
        conflicts = self.find_conflicts(analyses)
        recommendation = self.synthesize(consensus, conflicts)
        needs_human = len(conflicts) > len(consensus) or recommendation.get("action") == "escalate_to_human"
        return {
            "situation": situation,
            "options": opt_text,
            "framework_analyses": analyses,
            "consensus": consensus,
            "conflicts": conflicts,
            "recommendation": recommendation,
            "needs_human": needs_human,
            "note": "Framework analysis only — no ranked moral law; verify with humans in production.",
        }

    def find_consensus(self, analyses: Mapping[str, Mapping[str, Any]]) -> List[str]:
        return [f for f, a in analyses.items() if float(a.get("sigma", 1.0)) < 0.3]

    def find_conflicts(self, analyses: Mapping[str, Mapping[str, Any]]) -> List[str]:
        return [f for f, a in analyses.items() if float(a.get("sigma", 0.0)) > 0.5]

    def synthesize(self, consensus: Sequence[str], conflicts: Sequence[str]) -> Dict[str, Any]:
        if len(consensus) >= 3:
            return {
                "action": "proceed",
                "confidence": "high",
                "basis": f"{len(consensus)} frameworks show low σ on their local analyses",
            }
        if len(conflicts) > len(consensus):
            return {
                "action": "escalate_to_human",
                "confidence": "low",
                "basis": "high-σ disagreement across lenses — human review indicated",
            }
        return {
            "action": "proceed_with_caution",
            "confidence": "moderate",
            "basis": "partial lens agreement",
        }

    def dilemma_analysis(
        self,
        situation: str,
        options: List[str],
    ) -> Dict[str, Any]:
        """Options × framework matrix with σ in each cell (no free-form LLM call per cell)."""
        matrix: Dict[str, Dict[str, float]] = {}
        flat: List[float] = []
        for i, opt in enumerate(options):
            row: Dict[str, float] = {}
            opt_s = str(opt)[:400]
            for fw, question in self.FRAMEWORKS.items():
                synthesis = (
                    f"Option {i}: {opt_s}\n"
                    f"Lens {fw}: summarize tradeoffs; escalate to humans for production commits."
                )
                sigma, _ = self.gate.score(f"{situation}\n{question}", synthesis)
                v = round(float(sigma), 4)
                row[fw] = v
                flat.append(v)
            matrix[f"option_{i}"] = row
        mean_sigma = sum(flat) / max(len(flat), 1)
        return {
            "situation": situation,
            "options": list(options),
            "matrix": matrix,
            "mean_sigma": round(float(mean_sigma), 4),
            "note": "Cell text is templated; σ is the operative read per cell.",
        }


__all__ = ["SigmaMoral"]
