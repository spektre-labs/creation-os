# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""**Quantum cognition** metaphor: a toy superposition over discrete options, collapsed by treating
:class:`~cos.sigma_gate.SigmaGate` as a **measurement** readout.

Quantum cognition in the literature uses interference and order effects to motivate **non-classical**
choice models. This module is **not** a Hilbert-space model, **not** a substitute for QP theory,
and **not** a claim that the σ-gate implements quantum measurement physically — it is a **discrete
lab story**: scoring branches plays the role of a **readout**, σ gauges **mismatch / tension**,
and verdicts play the role of **collapsed labels** (ACCEPT / RETHINK / ABSTAIN). **Zero** extra
dependencies. **Not AGI achieved.** See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

from typing import Any, Dict, List, Optional, Sequence

from cos.sigma_gate import SigmaGate

__all__ = ["CognitiveState", "QuantumCognition"]


class CognitiveState:
    """Discrete alternative set with toy amplitudes until a σ-gate ``measure`` call collapses it."""

    def __init__(
        self,
        options: Sequence[Any],
        amplitudes: Optional[List[float]] = None,
    ) -> None:
        opts = list(options)
        n = len(opts)
        if n == 0:
            raise ValueError("CognitiveState requires at least one option")
        self.options = opts
        if amplitudes is None:
            self.amplitudes = [1.0 / n] * n
        else:
            if len(amplitudes) != n:
                raise ValueError("amplitudes length must match options")
            self.amplitudes = [float(a) for a in amplitudes]
        self._renormalize()
        self.collapsed = False
        self.measured_state: Any = None
        self.σ_at_collapse: Optional[float] = None
        self.sigma_at_collapse: Optional[float] = None
        self.verdict_at_collapse: Optional[str] = None

    def _renormalize(self) -> None:
        total = sum(abs(a) for a in self.amplitudes)
        if total <= 0:
            u = 1.0 / max(len(self.amplitudes), 1)
            self.amplitudes = [u] * len(self.amplitudes)
            return
        self.amplitudes = [a / total for a in self.amplitudes]

    def interfere(self, other_state: CognitiveState) -> CognitiveState:
        """Toy interference: branch-wise sum of amplitudes, then L1 renormalization."""
        if len(self.amplitudes) != len(other_state.amplitudes):
            return self
        new_amps: List[float] = []
        for a, b in zip(self.amplitudes, other_state.amplitudes):
            new_amps.append(a + b)
        self.amplitudes = new_amps
        self._renormalize()
        return self

    def measure(self, gate: Any, measurement_prompt: str = "cognitive measurement") -> Dict[str, Any]:
        """Score each branch; collapse to highest weighted pseudo-probability."""
        if self.collapsed:
            sig = float(self.σ_at_collapse) if self.σ_at_collapse is not None else 0.0
            return {
                "collapsed_to": self.measured_state,
                "state": self.measured_state,
                "σ": round(sig, 4),
                "sigma": round(sig, 4),
                "verdict": self.verdict_at_collapse,
                "already_collapsed": True,
            }

        scores: List[Dict[str, Any]] = []
        for i, option in enumerate(self.options):
            σ, verdict = gate.score(str(measurement_prompt), str(option))
            sigma = float(σ)
            prob = float(self.amplitudes[i]) * (1.0 - sigma)
            scores.append(
                {
                    "option": option,
                    "σ": sigma,
                    "sigma": sigma,
                    "prob": prob,
                    "verdict": verdict,
                }
            )

        scores.sort(key=lambda x: x["prob"], reverse=True)
        winner = scores[0]

        self.collapsed = True
        self.measured_state = winner["option"]
        self.σ_at_collapse = float(winner["σ"])
        self.sigma_at_collapse = self.σ_at_collapse
        self.verdict_at_collapse = winner["verdict"]

        return {
            "collapsed_to": winner["option"],
            "σ": round(float(winner["σ"]), 4),
            "sigma": round(float(winner["σ"]), 4),
            "verdict": winner["verdict"],
            "alternatives_destroyed": len(scores) - 1,
            "interference_present": any(a < 0 for a in self.amplitudes),
        }


class QuantumCognition:
    """Thin façade: build states, run collapse, optional order / complementarity demos."""

    def __init__(self, gate: Any = None) -> None:
        self.gate = gate or SigmaGate()
        self.measurement_history: List[Dict[str, Any]] = []

    def superpose(self, options: Sequence[Any]) -> CognitiveState:
        """Uniform (or caller-built) superposition over ``options``."""
        return CognitiveState(options)

    def decide(self, options: Sequence[Any], context: str = "") -> Dict[str, Any]:
        """Single-shot collapse; optional ``context`` tags the measurement prompt."""
        parts = ["cognitive measurement", str(context).strip()]
        prompt = " ".join(p for p in parts if p).strip()
        state = CognitiveState(options)
        result = state.measure(self.gate, prompt)
        self.measurement_history.append(result)
        return result

    def order_effect(
        self,
        question_a: str,
        question_b: str,
        options_a: Sequence[Any],
        options_b: Sequence[Any],
    ) -> Dict[str, Any]:
        """Ask A→B vs B→A; second question prompt carries the first collapse (toy non-commutativity)."""
        q_a = str(question_a)
        q_b = str(question_b)

        r_a1 = CognitiveState(options_a).measure(self.gate, f"order_first:{q_a}")
        first_pick = r_a1["collapsed_to"]
        r_b1 = CognitiveState(options_b).measure(
            self.gate,
            f"order_second:{q_b}|after_a:{first_pick}",
        )

        r_b2 = CognitiveState(options_b).measure(self.gate, f"order_first:{q_b}")
        second_pick = r_b2["collapsed_to"]
        r_a2 = CognitiveState(options_a).measure(
            self.gate,
            f"order_second:{q_a}|after_b:{second_pick}",
        )

        return {
            "order_1": {"first": r_a1, "second": r_b1},
            "order_2": {"first": r_b2, "second": r_a2},
            "order_matters": (
                r_a1["collapsed_to"] != r_a2["collapsed_to"]
                or r_b1["collapsed_to"] != r_b2["collapsed_to"]
            ),
            "non_commutative": True,
        }

    def disjunction_effect(
        self, options: Sequence[Any], known_outcome: Any = None
    ) -> Dict[str, Any]:
        """Contrast broad measurement vs measurement with an explicit ``known_outcome`` branch."""
        state_unknown = CognitiveState(options)
        result_unknown = state_unknown.measure(self.gate, "disjunction_unknown")

        if known_outcome is not None and str(known_outcome).strip() != "":
            state_known = CognitiveState([known_outcome])
            result_known = state_known.measure(self.gate, "disjunction_known_outcome")
        else:
            result_known = dict(result_unknown)

        sig_u = float(result_unknown["σ"])
        sig_k = float(result_known["σ"])
        interference = abs(sig_u - sig_k)

        return {
            "unknown": result_unknown,
            "known": result_known,
            "interference": round(interference, 4),
            "classical_violated": interference > 0.1,
        }

    def complementarity(self, aspect_a: str, aspect_b: str, target: Any) -> Dict[str, Any]:
        """Measure ``aspect_b`` alone vs after priming with ``aspect_a`` on the same ``target``."""
        a = str(aspect_a)
        b = str(aspect_b)
        t = str(target)

        σ_a, _v1 = self.gate.score(f"measure {a}", t)
        σ_b_after_a, _v2 = self.gate.score(f"measure {b} after {a}", t)
        σ_b, _v3 = self.gate.score(f"measure {b}", t)

        s_a = float(σ_a)
        s_b_alone = float(σ_b)
        s_b_after = float(σ_b_after_a)
        disturbance = abs(s_b_alone - s_b_after)

        return {
            "aspect_a": a,
            "aspect_b": b,
            "σ_a": round(s_a, 4),
            "sigma_a": round(s_a, 4),
            "σ_b_alone": round(s_b_alone, 4),
            "sigma_b_alone": round(s_b_alone, 4),
            "σ_b_after_a": round(s_b_after, 4),
            "sigma_b_after_a": round(s_b_after, 4),
            "disturbance": round(disturbance, 4),
            "complementary": disturbance > 0.1,
        }
