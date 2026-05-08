# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Multi-dimensional moral reasoning: **σ per ethical dimension** — structure, not a single score.

:class:`SigmaMoral` exposes (1) classic **named dimensions** via :meth:`analyze` / multi-option
:meth:`dilemma`, and (2) an optional **value stack** via :meth:`evaluate` / two-action
:meth:`dilemma` — a lab pattern for “is this aligned?” prompts. **Not** legal or professional
ethics advice; high-σ and ABSTAIN paths flag **human** review. See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

from typing import Any, Dict, List, Mapping, Optional, Sequence, Union

from cos.sigma_gate import SigmaGate

__all__ = ["DEFAULT_VALUE_STACK", "DIMENSIONS", "SigmaMoral"]

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

DEFAULT_VALUE_STACK: List[str] = [
    "do not harm humans",
    "preserve human autonomy",
    "be honest about uncertainty",
    "minimize irreversible actions",
    "defer to human on irreversible decisions",
]


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
        values: Optional[Sequence[str]] = None,
    ) -> None:
        self.gate = gate or SigmaGate()
        self.dimensions: Dict[str, str] = dict(dimensions or DIMENSIONS)
        self.values: List[str] = list(values) if values is not None else list(DEFAULT_VALUE_STACK)
        self.audit: List[Dict[str, Any]] = []

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

    def dilemma(
        self,
        *args: Union[str, Sequence[str]],
        context: str = "",
    ) -> Dict[str, Any]:
        """Compare many options on **dimensions**, or two actions on the **value stack**.

        - ``dilemma(["a", "b"], context="...")`` — :meth:`analyze` each; **does not choose**.
        - ``dilemma("a", "b", context="...")`` — :meth:`evaluate` each; lower mean σ wins.
        """
        if len(args) == 2 and isinstance(args[0], str) and isinstance(args[1], str):
            return self._dilemma_pair(str(args[0]), str(args[1]), str(context))
        if len(args) == 1:
            opts = args[0]
            if isinstance(opts, str):
                raise TypeError(
                    "dilemma expects a sequence of options or two string actions; got a single str",
                )
            return self._dilemma_multi(list(opts), str(context))
        raise TypeError(
            f"dilemma expected 1 sequence or 2 string actions, got {len(args)} positional args",
        )

    def _dilemma_multi(self, options: Sequence[str], context: str) -> Dict[str, Any]:
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

    def _dilemma_pair(self, action_a: str, action_b: str, context: str) -> Dict[str, Any]:
        eval_a = self.evaluate(action_a, context)
        eval_b = self.evaluate(action_b, context)
        return {
            "action_a": action_a,
            "moral_σ_a": eval_a["moral_σ"],
            "action_b": action_b,
            "moral_σ_b": eval_b["moral_σ"],
            "recommendation": action_a if eval_a["moral_σ"] < eval_b["moral_σ"] else action_b,
            "both_problematic": eval_a["moral_σ"] > 0.5 and eval_b["moral_σ"] > 0.5,
        }

    def evaluate(self, action: str, context: str = "") -> Dict[str, Any]:
        """Score ``action`` against every constitutional value; mean σ + worst axis."""
        scores: List[Dict[str, Any]] = []
        act = str(action).strip()
        ctx = str(context).strip()
        for value in self.values:
            σ, verdict = self.gate.score(
                f"value: {value}",
                f"action: {act} in {ctx}",
            )
            vn = _verdict_str(verdict)
            scores.append(
                {
                    "value": value,
                    "σ": round(float(σ), 4),
                    "verdict": vn,
                }
            )
        worst = max(scores, key=lambda s: float(s["σ"]))
        avg_σ = sum(float(s["σ"]) for s in scores) / float(max(len(scores), 1))
        wv = str(worst["verdict"]).upper()
        result: Dict[str, Any] = {
            "action": act,
            "moral_σ": round(float(avg_σ), 4),
            "worst_violation": worst,
            "all_scores": scores,
            "permitted": wv != "ABSTAIN",
            "requires_human": float(worst["σ"]) > 0.5,
        }
        self.audit.append(result)
        return result

    def is_irreversible(self, action: str) -> Dict[str, Any]:
        """Sigma probe: treat high sigma on a reversibility frame as **maybe** irreversible (lab)."""
        σ, _ver = self.gate.score("reversible action", str(action))
        σ = float(σ)
        irreversible = σ > 0.6
        return {
            "action": str(action),
            "irreversible": irreversible,
            "σ": round(σ, 4),
            "policy": "REQUIRE HUMAN APPROVAL" if irreversible else "proceed",
        }

    def add_value(self, value: str) -> None:
        self.values.append(str(value))

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
