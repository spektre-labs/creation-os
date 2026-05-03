# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-explain v2 — short natural-language rationales, counterfactuals, attributions (lab).

Not a substitute for mechanistic interpretability. See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import re
from typing import Any, Dict, List, Mapping, Optional

__all__ = ["SigmaExplain"]


class SigmaExplain:
    """Human-readable explanations tied to σ-gate outcomes."""

    def __init__(self, gate: Any = None) -> None:
        from cos.sigma_gate import SigmaGate

        self.gate = gate or SigmaGate()

    def explain(self, prompt: str, response: str, sigma: float, verdict: str) -> Dict[str, Any]:
        """Plain-language summary (short)."""
        v = str(verdict).upper()
        s = float(sigma)
        if v == "ACCEPT":
            msg = "The gate sees this answer as consistent enough with the prompt at the current τ."
        elif v == "RETHINK":
            msg = "The gate suggests reviewing or regenerating: uncertainty or mismatch is elevated."
        else:
            msg = "The gate recommends abstaining: the pair looks unreliable or underspecified for a confident answer."
        return {
            "summary": msg,
            "sigma": round(s, 6),
            "verdict": v,
            "natural_language": True,
        }

    def counterfactual(self, prompt: str, response: str, gate: Optional[Any] = None) -> Dict[str, Any]:
        """One-step tweak: replace last word with a neutral token and rescore."""
        g = gate or self.gate
        base_s, base_v = g.score(str(prompt), str(response))
        toks = str(response).split()
        if len(toks) < 2:
            alt = str(response) + " (clarified)"
        else:
            alt = " ".join(toks[:-1] + ["possibly"])
        alt_s, alt_v = g.score(str(prompt), alt)
        return {
            "original_sigma": round(float(base_s), 6),
            "counterfactual_sigma": round(float(alt_s), 6),
            "change": "last_token→'possibly'",
            "verdict_before": str(base_v),
            "verdict_after": str(alt_v),
        }

    def token_attribution(self, response: str, gate: Optional[Any] = None) -> Dict[str, Any]:
        """Leave-one-token-out approximations (cheap lab attribution)."""
        g = gate or self.gate
        full = str(response)
        base_s, _ = g.score("", full)
        toks = re.findall(r"\S+|\s+", full)
        words = [t for t in toks if t.strip()]
        deltas: List[Dict[str, Any]] = []
        for i, w in enumerate(words):
            alt_list = words[:i] + words[i + 1 :]
            alt = " ".join(alt_list)
            s, _ = g.score("", alt if alt else "[empty]")
            deltas.append({"token": w, "delta_sigma": round(float(base_s) - float(s), 6)})
        top = sorted(deltas, key=lambda x: abs(x["delta_sigma"]), reverse=True)[:5]
        return {"per_token": deltas, "top_influencers": top, "baseline_sigma": round(float(base_s), 6)}

    def feature_importance(self, probe_scores: Mapping[str, float]) -> Dict[str, Any]:
        """Which named probe scored highest (treat as stress contributor)."""
        if not probe_scores:
            return {"top": None, "spread": 0.0}
        items = sorted(((k, float(v)) for k, v in probe_scores.items()), key=lambda x: -x[1])
        top, second = items[0], items[1] if len(items) > 1 else (items[0], ("", 0.0))
        spread = top[1] - second[1]
        return {"top": {"name": top[0], "value": round(top[1], 6)}, "spread": round(spread, 6)}

    def contrastive(self, good_response: str, bad_response: str, gate: Optional[Any] = None) -> Dict[str, Any]:
        """Compare ACCEPT vs RETHINK-style separation on two completions."""
        g = gate or self.gate
        sg, vg = g.score("contrastive", str(good_response))
        sb, vb = g.score("contrastive", str(bad_response))
        return {
            "good": {"sigma": round(float(sg), 6), "verdict": str(vg)},
            "bad": {"sigma": round(float(sb), 6), "verdict": str(vb)},
            "why": "Lower σ on 'good' under the same synthetic prompt label means the gate prefers that surface form.",
        }

    def confidence_in_explanation(self, probe_spread: float, sigma: float) -> Dict[str, Any]:
        """How much to trust this explanation proxy (internal consistency)."""
        spread = float(max(0.0, probe_spread))
        s = float(sigma)
        conf = max(0.0, min(1.0, 1.0 - 0.5 * spread - 0.3 * abs(s - 0.5)))
        return {"confidence": round(conf, 6), "probe_spread": spread, "sigma": s}
