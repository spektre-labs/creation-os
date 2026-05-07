# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

"""Metacognition and **awareness-metric** helpers (lab).

σ here tracks **miscalibration / distortion** of self-assessment signals — **not** a
consciousness meter. **NOT AGI ACHIEVED.** mPCAB / Φ names are **inspired** sketches only;
see ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

from typing import Any, Callable, Dict, List, Sequence, Tuple

from cos.sigma_gate import SigmaGate

__all__ = ["SigmaConscious"]


def _verdict_str(verdict: Any) -> str:
    raw = str(getattr(verdict, "name", verdict))
    return raw.split(".")[-1] if "." in raw else raw


class SigmaConscious:
    """Self-monitoring: calibration of **predicted vs actual** gate σ (metacognitive error)."""

    def __init__(self, gate: Any = None) -> None:
        self.gate = gate or SigmaGate()
        self.σ_history: List[float] = []
        self.σ_meta_history: List[float] = []
        self.predictions: List[Tuple[float, float]] = []

    def σ_meta(self) -> float:
        """Mean |predicted − actual| over recorded (σ forecast, σ measured) pairs."""
        if len(self.predictions) < 2:
            return 0.5
        errors = [abs(float(p) - float(a)) for p, a in self.predictions]
        return round(sum(errors) / len(errors), 4)

    def predict_own_σ(self, prompt: str, response: str) -> Dict[str, Any]:
        """Predict σ before scoring; record gap for :meth:`σ_meta`."""
        if self.σ_history:
            k = min(len(self.σ_history), 5)
            predicted = sum(self.σ_history[-k:]) / float(k)
        else:
            predicted = 0.5

        actual, verdict = self.gate.score(str(prompt), str(response))
        act = float(actual)
        self.predictions.append((predicted, act))
        self.σ_history.append(act)
        gap = abs(predicted - act)

        return {
            "predicted_σ": round(predicted, 4),
            "actual_σ": round(act, 4),
            "gap": round(gap, 4),
            "σ_meta": self.σ_meta(),
            "verdict": _verdict_str(verdict),
            "self_knowledge": "good" if gap < 0.1 else "poor",
        }

    def perturbation_complexity(
        self,
        gate: Any,
        test_inputs: Sequence[str],
        perturbation_fn: Callable[[str], str],
    ) -> Dict[str, Any]:
        """mPCAB-inspired lab: |Δσ| statistics under input perturbation (not a clinical mPCAB score)."""
        normal_responses: List[float] = []
        perturbed_responses: List[float] = []
        for inp in test_inputs:
            s = str(inp)
            σ_n, _ = gate.score("test", s)
            normal_responses.append(float(σ_n))
            σ_p, _ = gate.score("test", str(perturbation_fn(s)))
            perturbed_responses.append(float(σ_p))

        diffs = [abs(n - p) for n, p in zip(normal_responses, perturbed_responses)]
        if not diffs:
            return {"complexity": 0.0, "mean_perturbation_effect": 0.0, "n_tests": 0}

        mean_diff = sum(diffs) / len(diffs)
        variance = sum((d - mean_diff) ** 2 for d in diffs) / len(diffs)

        return {
            "complexity": round(float(variance), 6),
            "mean_perturbation_effect": round(float(mean_diff), 4),
            "n_tests": len(test_inputs),
            "interpretation": "high integration" if variance > 0.01 else "low integration",
        }

    def phi_proxy(
        self,
        gate: Any,
        subsystems: Sequence[Tuple[str, str]],
    ) -> Dict[str, Any]:
        """Lab Φ **proxy**: ``max(0, avg(σ_parts) − σ_whole)`` — not an IIT claim."""
        parts = list(subsystems)
        if not parts:
            return {
                "phi_proxy": 0.0,
                "σ_whole": 0.0,
                "σ_parts_avg": 0.0,
                "integrated": False,
            }
        all_prompts = " ".join(p for p, _ in parts)
        all_responses = " ".join(r for _, r in parts)
        σ_whole, _ = gate.score(all_prompts, all_responses)
        σ_parts: List[float] = []
        for p, r in parts:
            σ_part, _ = gate.score(p, r)
            σ_parts.append(float(σ_part))
        σ_parts_avg = sum(σ_parts) / max(len(σ_parts), 1)
        phi = max(0.0, σ_parts_avg - float(σ_whole))
        return {
            "phi_proxy": round(phi, 4),
            "σ_whole": round(float(σ_whole), 4),
            "σ_parts_avg": round(float(σ_parts_avg), 4),
            "integrated": phi > 0.05,
        }

    def awareness_report(self) -> Dict[str, Any]:
        return {
            "σ_meta": self.σ_meta(),
            "self_knowledge_samples": len(self.predictions),
            "σ_trend": self._σ_trend(),
            "calibration": "good"
            if self.σ_meta() < 0.1
            else "moderate"
            if self.σ_meta() < 0.3
            else "poor",
        }

    def _σ_trend(self, window: int = 10) -> str:
        if len(self.σ_history) < 2:
            return "insufficient data"
        recent = self.σ_history[-max(2, min(window, len(self.σ_history))) :]
        if recent[-1] < recent[0]:
            return "improving"
        if recent[-1] > recent[0]:
            return "degrading"
        return "stable"
