# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""**Symbol grounding** sketch: treat :class:`~cos.sigma_gate.SigmaGate` σ as a *compatibility*
score between a **symbol** (or utterance) and a **referent** (evidence, denotation, outcome).

Harnad’s formulation asks how intrinsic semantics could emerge for an artificial system. This
module does **not** close that philosophical problem and does **not** replace world models or
sensorimotor learning — it is a **discrete, σ-gated** lab story: low σ with an acceptable
verdict **stands in for** “grounded”; high σ / ABSTAIN **stands in for** an “ungrounded” or
hallucination-leaning gap. **Zero** extra dependencies. **Not AGI achieved.** See
``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

from typing import Any, Dict, List, Sequence, Tuple, Union

from cos.sigma_gate import ACCEPT, ABSTAIN, SigmaGate

__all__ = ["SigmaGrounding"]


class SigmaGrounding:
    """Symbol–referent checks through :class:`SigmaGate` (verdict + σ)."""

    def __init__(self, gate: Any = None) -> None:
        self.gate = gate or SigmaGate()
        self.groundings: Dict[str, Dict[str, Any]] = {}

    @staticmethod
    def _sym_key(symbol: Any) -> str:
        return str(symbol)

    def ground(self, symbol: Any, referent: Any) -> Dict[str, Any]:
        """Score ``symbol`` vs ``referent``; cache result under ``str(symbol)``."""
        key = self._sym_key(symbol)
        sigma, verdict = self.gate.score(str(symbol), str(referent))
        sigma_f = float(sigma)
        grounded = verdict == ACCEPT

        self.groundings[key] = {
            "symbol": key,
            "referent": referent,
            "σ": round(sigma_f, 4),
            "sigma": round(sigma_f, 4),
            "grounded": grounded,
            "verdict": verdict,
        }
        return dict(self.groundings[key])

    def is_grounded(self, symbol: Any) -> Dict[str, Any]:
        """Return cached grounding record, or a never-grounded placeholder."""
        key = self._sym_key(symbol)
        if key not in self.groundings:
            return {"grounded": False, "reason": "never grounded", "symbol": key}
        return dict(self.groundings[key])

    def hallucination_check(self, statement: Any, evidence: Any) -> Dict[str, Any]:
        """σ(evidence, statement): does evidence support the statement (lite pair ordering)?"""
        sigma, verdict = self.gate.score(str(evidence), str(statement))
        sigma_f = float(sigma)
        return {
            "statement": str(statement)[:100],
            "σ": round(sigma_f, 4),
            "sigma": round(sigma_f, 4),
            "grounded": verdict == ACCEPT,
            "hallucinating": verdict == ABSTAIN,
            "verdict": verdict,
        }

    def ground_hierarchy(
        self, concept_chain: Sequence[Tuple[Any, Any]]
    ) -> Dict[str, Any]:
        """Chain local (symbol → referent) checks; strength ≈ weakest σ link."""
        chain_results: List[Dict[str, Any]] = []
        cumulative = 0.0

        for symbol, referent in concept_chain:
            result = self.ground(symbol, referent)
            cumulative += float(result["σ"])
            chain_results.append(
                {
                    "symbol": str(symbol),
                    "referent": referent,
                    "σ": result["σ"],
                    "grounded": result["grounded"],
                    "cumulative_σ": round(cumulative, 4),
                    "cumulative_sigma": round(cumulative, 4),
                }
            )

        weakest = max(chain_results, key=lambda x: x["σ"])
        all_grounded = all(r["grounded"] for r in chain_results)
        n = max(len(chain_results), 1)

        return {
            "chain": chain_results,
            "all_grounded": all_grounded,
            "weakest_link": weakest["symbol"],
            "weakest_σ": weakest["σ"],
            "weakest_sigma": weakest["σ"],
            "avg_σ": round(cumulative / n, 4),
            "avg_sigma": round(cumulative / n, 4),
        }

    def sensorimotor_ground(self, symbol: Any, action_result: Any) -> Dict[str, Any]:
        """Embodied toy: score predicted framing vs observed action outcome."""
        left = f"action '{symbol}' should produce"
        sigma, verdict = self.gate.score(left, str(action_result))
        sigma_f = float(sigma)
        return {
            "symbol": str(symbol),
            "action_result": str(action_result)[:100],
            "σ": round(sigma_f, 4),
            "sigma": round(sigma_f, 4),
            "embodied": True,
            "grounded": verdict == ACCEPT,
            "verdict": verdict,
        }

    def drift_check(self, symbol: Any) -> Dict[str, Any]:
        """Re-score cached pair; positive Δσ above tolerance ⇒ drift flag."""
        key = self._sym_key(symbol)
        if key not in self.groundings:
            return {"drifted": False, "reason": "not grounded", "symbol": key}

        original = self.groundings[key]
        sigma_now, _verdict = self.gate.score(str(key), str(original["referent"]))
        sigma_now_f = float(sigma_now)
        orig_sigma = float(original["σ"])
        drift = sigma_now_f - orig_sigma

        return {
            "symbol": key,
            "σ_original": original["σ"],
            "sigma_original": original["σ"],
            "σ_now": round(sigma_now_f, 4),
            "sigma_now": round(sigma_now_f, 4),
            "drift": round(drift, 4),
            "drifted": drift > 0.1,
        }

    def ungrounded_symbols(self) -> List[Dict[str, Union[str, float]]]:
        """Symbols whose last cached check was not ACCEPT."""
        return [
            {"symbol": s, "σ": g["σ"], "sigma": g["σ"]}
            for s, g in self.groundings.items()
            if not g["grounded"]
        ]
