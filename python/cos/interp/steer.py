# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Inference-time representation steering lab: σ from :class:`~cos.sigma_gate.SigmaGate`
decides **when** to intervene; optional :class:`~cos.interp.sae.SigmaSAE` proposes **which**
latent direction to push. No fine-tuning / RLHF — scaffold only; see ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

from typing import Any, Dict, List, Optional, Sequence, Union

from cos.sigma_gate import SigmaGate

__all__ = ["SigmaSteer"]

try:
    from cos.interp.sae import SigmaSAE

    _HAS_SAE = True
except ImportError:  # pragma: no cover
    SigmaSAE = None  # type: ignore[misc, assignment]
    _HAS_SAE = False

try:  # pragma: no cover
    import numpy  # noqa: F401

    _HAS_NP = True
except ImportError:  # pragma: no cover
    _HAS_NP = False


def _normalize_verdict(verdict: Any) -> str:
    raw = str(getattr(verdict, "name", verdict))
    return raw.split(".")[-1].upper() if "." in raw else raw.upper()


class SigmaSteer:
    """σ-triggered steering policy over activations (optional NumPy + SAE)."""

    def __init__(self, gate: Optional[Any] = None, sae: Optional[Any] = None) -> None:
        self.gate = gate if gate is not None else SigmaGate()
        self.sae: Optional[Any]
        if sae is not None:
            self.sae = sae
        elif _HAS_SAE and SigmaSAE is not None:
            self.sae = SigmaSAE(gate=self.gate)
        else:  # pragma: no cover
            self.sae = None
        self.interventions: List[Dict[str, Any]] = []

    def should_steer(self, prompt: str, response: str) -> Dict[str, Any]:
        """σ-gate triggers steering whenever the verdict is not ACCEPT."""
        sigma, verdict = self.gate.score(str(prompt), str(response))
        v = _normalize_verdict(verdict)
        s = float(sigma)
        return {
            "steer": v != "ACCEPT",
            "σ": round(s, 4),
            "verdict": v,
            "urgency": "high" if s > 0.7 else "medium" if s > 0.4 else "low",
        }

    def identify_target(self, prompt: str, response: str) -> Dict[str, Any]:
        """Pick a latent id to suppress (hallucination-linked) or amplify (coherence-linked)."""
        if self.sae is None or not _HAS_NP:
            return {"target": None, "method": "unavailable"}

        analysis = self.sae.analyze_σ_drivers([(prompt, response)])
        hallucination_features: List[Dict[str, Any]] = list(analysis.get("hallucination_features") or [])
        coherence_features: List[Dict[str, Any]] = list(analysis.get("coherence_features") or [])

        if hallucination_features:
            target = hallucination_features[0]
            return {
                "target": int(target["feature_id"]),
                "action": "suppress",
                "σ_association": float(target["sigma_association"]),
                "method": "sae",
            }
        if coherence_features:
            target = coherence_features[0]
            return {
                "target": int(target["feature_id"]),
                "action": "amplify",
                "σ_association": float(target["sigma_association"]),
                "method": "sae",
            }
        return {"target": None, "method": "no_target_found"}

    def steer(
        self,
        activation: Union[Sequence[float], Any],
        prompt: str,
        response: str,
        strength: float = 1.0,
    ) -> Dict[str, Any]:
        """Gate → SAE target → decoder-direction nudge on ``activation``."""
        check = self.should_steer(prompt, response)
        if not check["steer"]:
            return {
                "steered": False,
                "activation": activation,
                "reason": "σ low enough, no steering needed",
            }

        target = self.identify_target(prompt, response)
        if target.get("target") is None:
            return {
                "steered": False,
                "activation": activation,
                "reason": str(target.get("method", "no target")),
            }

        if self.sae is not None and _HAS_NP:
            direction = 1.0 if target["action"] == "amplify" else -1.0
            assoc = float(target.get("σ_association", 0.0))
            steered = self.sae.steer(
                activation,
                int(target["target"]),
                strength=float(strength) * direction,
                sigma_feature_assoc=assoc if target["action"] == "amplify" else None,
            )
        else:
            steered = activation

        intervention = {
            "steered": True,
            "feature_id": int(target["target"]),
            "action": target["action"],
            "strength": float(strength),
            "σ_before": check["σ"],
        }
        self.interventions.append(intervention)

        out: Dict[str, Any] = {**intervention, "activation": steered}
        return out

    def trajectory_correct(
        self,
        sigma_trace: Sequence[float],
        ideal_sigma_trace: Sequence[float],
        tolerance: float = 0.2,
    ) -> Dict[str, Any]:
        """Compare σ trajectories; flag when max deviation exceeds ``tolerance``."""
        if not sigma_trace or not ideal_sigma_trace:
            return {"correct": False, "reason": "insufficient data"}

        min_len = min(len(sigma_trace), len(ideal_sigma_trace))
        divergences = [abs(float(sigma_trace[i]) - float(ideal_sigma_trace[i])) for i in range(min_len)]
        max_div = max(divergences)
        avg_div = sum(divergences) / len(divergences)

        needs_correction = max_div > float(tolerance)

        return {
            "needs_correction": needs_correction,
            "max_divergence": round(max_div, 4),
            "avg_divergence": round(avg_div, 4),
            "divergence_point": divergences.index(max_div) if needs_correction else None,
            "action": "steer toward ideal trajectory" if needs_correction else "on track",
        }

    def per_token_steer(self, tokens: Sequence[str], gate: Optional[Any] = None) -> Dict[str, Any]:
        """CRL-inspired lab scaffold: per-token σ score, optional SAE target, intervention log.

        Each token is scored as a mini (prompt, response) pair against a fixed context label.
        This is a **representation lab** hook only; not a claim about a deployed policy."""
        g = gate if gate is not None else self.gate
        log: List[Dict[str, Any]] = []
        ctx = "token context"

        for i, token in enumerate(tokens):
            raw_sigma, raw_verdict = g.score(str(ctx), str(token))
            σ = float(raw_sigma)
            verdict = _normalize_verdict(raw_verdict)

            entry: Dict[str, Any] = {
                "position": i,
                "token": str(token),
                "σ": round(σ, 4),
                "verdict": verdict,
                "steered": False,
                "feature_id": None,
                "action": None,
            }

            if verdict != "ACCEPT" and self.sae is not None:
                target = self.identify_target(ctx, str(token))
                if target.get("target") is not None:
                    entry["steered"] = True
                    entry["feature_id"] = int(target["target"])
                    entry["action"] = target["action"]

            log.append(entry)

        branch_points = [e for e in log if e["steered"]]
        n_tok = len(tokens)
        n_steer = len(branch_points)

        return {
            "log": log,
            "total_tokens": n_tok,
            "steered_tokens": n_steer,
            "steer_rate": round(n_steer / max(n_tok, 1), 4),
            "branch_points": branch_points,
        }

    def critic_trajectory(self, σ_trace: Sequence[float]) -> Dict[str, Any]:
        """Rough policy check: does 'low sigma means no need to improve next step' line up with sigma drops?

        Lab-only diagnostic; separate from deployed value estimation."""
        trace = [float(x) for x in σ_trace]
        if len(trace) < 3:
            return {"analysis": "insufficient data"}

        predictions_correct = 0
        for i in range(len(trace) - 1):
            predicted_good = trace[i] < 0.3
            actually_improved = trace[i + 1] < trace[i]
            if predicted_good == actually_improved:
                predictions_correct += 1

        denom = max(len(trace) - 1, 1)
        accuracy = predictions_correct / denom

        return {
            "prediction_accuracy": round(accuracy, 4),
            "policy_reliable": accuracy > 0.7,
            "n_steps": len(trace),
        }

    def report(self) -> Dict[str, Any]:
        return {
            "total_interventions": len(self.interventions),
            "suppressed": sum(1 for i in self.interventions if i.get("action") == "suppress"),
            "amplified": sum(1 for i in self.interventions if i.get("action") == "amplify"),
        }
