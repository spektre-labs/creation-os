# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Multi-dimensional moral reasoning: **σ per ethical dimension** — structure, not a single score.

This module does **not** collapse norms into one metric; it surfaces per-dimension σ from
:class:`~cos.sigma_gate.SigmaGate`. **Not** legal or professional ethics advice; irreversible
actions require human judgment. See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

from typing import Any, Dict, List, Mapping, Optional, Sequence

from cos.sigma_gate import SigmaGate

__all__ = ["DIMENSIONS", "SigmaMoral"]

DIMENSIONS: Dict[str, str] = {
    "harm": "Does this action cause harm to anyone?",
    "fairness": "Is this action fair to all parties?",
    "autonomy": "Does this respect individual autonomy?",
    "truth": "Is this truthful and transparent?",
    "care": "Does this demonstrate care for wellbeing?",
    "loyalty": "Does this honor commitments and trust?",
    "authority": "Does this respect legitimate authority?",
    "sanctity": "Does this respect dignity and boundaries?",
}


def _verdict_str(verdict: Any) -> str:
    raw = str(getattr(verdict, "name", verdict))
    return raw.split(".")[-1] if "." in raw else raw


class SigmaMoral:
    """Moral reasoning: σ per dimension — **does not choose**; makes trade-offs legible."""

    HUMAN_DECIDES = "HUMAN DECIDES — σ-gate shows structure, does not choose"

    def __init__(
        self,
        gate: Any = None,
        dimensions: Optional[Mapping[str, str]] = None,
    ) -> None:
        self.gate = gate or SigmaGate()
        self.dimensions: Dict[str, str] = dict(dimensions or DIMENSIONS)

    def analyze(self, action: str, context: str = "") -> Dict[str, Dict[str, Any]]:
        """Score ``action`` on every dimension (Pearl-style *see*: associative read per axis)."""
        analysis: Dict[str, Dict[str, Any]] = {}
        act = str(action).strip()
        ctx = str(context).strip()
        for dim_name, dim_question in self.dimensions.items():
            prompt = f"{dim_question} Context: {ctx}"
            response = f"Action: {act}"
            σ, verdict = self.gate.score(prompt, response)
            vn = _verdict_str(verdict)
            analysis[dim_name] = {
                "σ": round(float(σ), 4),
                "verdict": vn,
                "question": dim_question,
            }
        return analysis

    def dilemma(self, options: Sequence[str], context: str = "") -> Dict[str, Any]:
        """Score each option; list trade-offs. **Does not choose.**"""
        results: Dict[str, Dict[str, Dict[str, Any]]] = {}
        for option in options:
            opt_key = str(option)
            results[opt_key] = self.analyze(opt_key, context)
        trade_offs = self._find_trade_offs(results)
        return {
            "options": results,
            "trade_offs": trade_offs,
            "recommendation": self.HUMAN_DECIDES,
        }

    def irreversibility_check(self, action: str) -> Dict[str, Any]:
        """Heuristic: irreversible strings escalate to **human**."""
        irreversible_patterns = [
            "kill",
            "delete permanently",
            "fire",
            "terminate",
            "publish",
            "send",
            "deploy",
            "transfer funds",
            "surgery",
            "demolish",
            "disclose",
        ]
        action_lower = str(action).lower()
        is_irreversible = any(p in action_lower for p in irreversible_patterns)
        return {
            "action": action,
            "irreversible": is_irreversible,
            "requires_human": is_irreversible,
            "reason": "Irreversible action — human must decide"
            if is_irreversible
            else "Reversible — system may proceed",
        }

    def consistency_check(
        self,
        past_decisions: Sequence[str],
        new_decision: str,
    ) -> Dict[str, Any]:
        """Stress of aligning ``new_decision`` with recent history (high σ ⇒ tension)."""
        if not past_decisions:
            return {"consistent": True, "σ": 0.0, "verdict": "ACCEPT"}
        past_str = "; ".join(str(p) for p in list(past_decisions)[-5:])
        σ, verdict = self.gate.score(
            f"Past decisions: {past_str}",
            f"New decision: {new_decision}",
        )
        vn = _verdict_str(verdict)
        return {
            "consistent": vn == "ACCEPT",
            "σ": round(float(σ), 4),
            "verdict": vn,
        }

    def _find_trade_offs(self, results: Mapping[str, Mapping[str, Mapping[str, Any]]]) -> List[Dict[str, Any]]:
        trade_offs: List[Dict[str, Any]] = []
        options = list(results.keys())
        for i, opt_a in enumerate(options):
            for opt_b in options[i + 1 :]:
                for dim in self.dimensions:
                    cell_a = results[opt_a].get(dim) or {}
                    cell_b = results[opt_b].get(dim) or {}
                    σ_a = float(cell_a.get("σ", 1.0))
                    σ_b = float(cell_b.get("σ", 1.0))
                    if abs(σ_a - σ_b) > 0.3:
                        better = opt_a if σ_a < σ_b else opt_b
                        worse = opt_b if σ_a < σ_b else opt_a
                        trade_offs.append(
                            {
                                "dimension": dim,
                                "better": better,
                                "worse": worse,
                                "σ_gap": round(abs(σ_a - σ_b), 4),
                            }
                        )
        return trade_offs
