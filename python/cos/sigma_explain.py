# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
v175 σ-explain: human-readable verdict rationales + token ablation prior (lab).

Optional coupling to :class:`SigmaSAELabTopK` for L5-style feature bullets.
"""
from __future__ import annotations

from typing import Any, Dict, List, Optional


class SigmaExplain:
    def __init__(self, gate: Any, sae: Any = None, model: Any = None) -> None:
        self.gate = gate
        self.sae = sae
        self.model = model

    def explain(
        self,
        prompt: str,
        response: str,
        sigma: float,
        verdict: str,
    ) -> Dict[str, Any]:
        token_sigmas = self.token_attribution(prompt, response)
        feature_explanation: Optional[Dict[str, Any]] = None
        if self.sae is not None:
            from cos.sigma_sae import lab_activation_from_text

            dim = getattr(self.sae, "input_dim", 32)
            if hasattr(self.sae, "explain_sigma"):
                try:
                    activation = lab_activation_from_text(prompt, response, int(dim))
                    feature_explanation = self.sae.explain_sigma(activation, float(sigma))
                except Exception:
                    feature_explanation = None
        counterfactual = self.counterfactual_explanation(prompt, response, float(sigma))
        natural = self.natural_language_explanation(str(verdict), float(sigma), token_sigmas, feature_explanation)
        return {
            "sigma": float(sigma),
            "verdict": verdict,
            "natural_explanation": natural,
            "token_attributions": token_sigmas[:5],
            "features": feature_explanation,
            "counterfactual": counterfactual,
        }

    def natural_language_explanation(
        self,
        verdict: str,
        sigma: float,
        tokens: List[Dict[str, Any]],
        features: Optional[Dict[str, Any]],
    ) -> str:
        if verdict == "ACCEPT":
            return f"σ={sigma:.2f}: response passes inexpensive coherence checks across ablated spans."
        if verdict == "RETHINK":
            high_tokens = [t for t in tokens if float(t.get("sigma", 0.0)) > 0.5]
            toks = ", ".join(str(t.get("token")) for t in high_tokens[:3])
            return (
                f"σ={sigma:.2f}: uncertainty spike on ablations. "
                f"Tokens tied to high σ: {toks or '(none flagged)'}. Consider retrieval or rewrite."
            )
        if verdict == "ABSTAIN":
            reasons: List[str] = []
            if features and features.get("likely_causes"):
                reasons = [str(c.get("label")) for c in features["likely_causes"][:2]]
            return (
                f"σ={sigma:.2f}: abstain region. "
                f"Likely causes: {', '.join(reasons) if reasons else 'high aggregate uncertainty'}."
            )
        return f"σ={sigma:.2f}: verdict={verdict} (lab explanation)."

    def token_attribution(self, prompt: str, response: str) -> List[Dict[str, Any]]:
        tokens = response.split()
        attributions: List[Dict[str, Any]] = []
        if not tokens:
            return attributions
        sigma_with, _ = self.gate.score(prompt, response)
        for i, token in enumerate(tokens):
            without = " ".join(tokens[:i] + tokens[i + 1 :]) or "(empty)"
            sigma_without, _ = self.gate.score(prompt, without)
            attribution = float(sigma_with) - float(sigma_without)
            attributions.append(
                {
                    "token": token,
                    "position": i,
                    "sigma": float(sigma_with),
                    "attribution": attribution,
                }
            )
        attributions.sort(key=lambda a: abs(float(a["attribution"])), reverse=True)
        return attributions

    def counterfactual_explanation(self, prompt: str, response: str, sigma: float) -> Dict[str, Any]:
        if float(sigma) < 0.3:
            return {"needed": False}
        if self.model is None:
            return {"needed": True, "suggestions": []}
        suggestions: List[Dict[str, Any]] = []
        for temp in (0.3, 0.1):
            try:
                alt = self.model.generate(prompt, temperature=temp)
            except TypeError:
                alt = self.model.generate(f"{prompt} [alt temp {temp}]")
            alt_sigma, alt_verdict = self.gate.score(prompt, alt)
            if float(alt_sigma) < float(sigma):
                suggestions.append(
                    {
                        "alternative": alt[:160],
                        "sigma": float(alt_sigma),
                        "verdict": str(alt_verdict),
                        "improvement": float(sigma) - float(alt_sigma),
                    }
                )
        return {"needed": True, "suggestions": suggestions}


__all__ = ["SigmaExplain"]
