# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tiny σ-scored **factor graph** + **round-robin message update** (lab toy).

:class:`~cos.sigma_gate.SigmaGate` plays the role of a unary “compatibility” potential on
concatenated variable strings. This is **not** a general variational-Bayes factorization, **not**
guaranteed to converge to a Bethe free energy fixed point, and **not** the reactive message-passing
engine of van de Laar *et al.* in full generality — it is a **discrete, σ-gated** sketch that
lets Ω / active-inference layers share a **local parallel** narrative (**not** a monolithic
pipeline claim in the strict sense). Factor graphs pair with :mod:`cos.predictive` only as
**composable illustrations** of message vs hierarchy-error stories — not a closed Friston
implementation. **Zero** third-party deps beyond the gate. **Not AGI
achieved.** See ``docs/CLAIM_DISCIPLINE.md``.
"""
from __future__ import annotations

from typing import Any, Dict, List, Optional, Sequence, Tuple

from cos.sigma_gate import SigmaGate

__all__ = ["Variable", "Factor", "SigmaFactorGraph"]


class Variable:
    """Variable node storing a scalar belief and factor → variable messages."""

    def __init__(self, name: str, value: Any = None) -> None:
        self.name = str(name)
        self.value = value
        self.belief = 0.5
        self.messages_in: Dict[str, float] = {}

    def update_belief(self) -> None:
        """Belief ≈ mean of incoming σ-messages."""
        if not self.messages_in:
            return
        vals = list(self.messages_in.values())
        self.belief = sum(vals) / len(vals)

    def send_message(self, exclude_factor: Optional[str] = None) -> float:  # noqa: ARG002
        """Return current belief as the outbound summary (toy)."""
        return float(self.belief)


class Factor:
    """Factor potential over variables; σ = gate violation readout."""

    def __init__(self, name: str, gate: Any, variables: List[Variable]) -> None:
        self.name = str(name)
        self.gate = gate
        self.variables = list(variables)
        self.sigma: float = 0.5

    def evaluate(self) -> float:
        """Score combined assignment + mean-belief tag (so passing can reshape σ)."""
        values: List[str] = [str(v.value if v.value is not None else v.name) for v in self.variables]
        combined = " ".join(values)
        if self.variables:
            mb = sum(float(v.belief) for v in self.variables) / len(self.variables)
            combined = f"{combined} μb={mb:.4f}"
        sig, _verdict = self.gate.score(self.name, combined)
        self.sigma = float(sig)
        return self.sigma

    def send_messages(self) -> None:
        """Push σ-weighted messages to each incident variable."""
        self.evaluate()
        s = float(self.sigma)
        for var in self.variables:
            other_beliefs = [float(v.belief) for v in self.variables if v is not var]
            if other_beliefs:
                w = sum(other_beliefs) / len(other_beliefs)
                msg = s * w
            else:
                msg = s
            var.messages_in[self.name] = float(msg)


class SigmaFactorGraph:
    """Named variables + factors; :meth:`infer` loops factor→messages then belief refresh."""

    def __init__(self, gate: Any = None) -> None:
        self.gate = gate or SigmaGate()
        self.variables: Dict[str, Variable] = {}
        self.factors: Dict[str, Factor] = {}

    def add_variable(self, name: str, value: Any = None) -> Variable:
        var = Variable(name, value)
        self.variables[str(name)] = var
        return var

    def add_factor(self, name: str, variable_names: Sequence[str]) -> Factor:
        vs = [self.variables[n] for n in variable_names]
        fac = Factor(name, self.gate, vs)
        self.factors[str(name)] = fac
        return fac

    def vfe(self) -> float:
        """Mean factor σ after :meth:`Factor.evaluate` (discrete VFE *proxy*)."""
        if not self.factors:
            return 0.0
        tot = 0.0
        for f in self.factors.values():
            tot += float(f.evaluate())
        return tot / len(self.factors)

    def _snapshot(self) -> Dict[str, Tuple[Any, float, Dict[str, float]]]:
        snap: Dict[str, Tuple[Any, float, Dict[str, float]]] = {}
        for n, v in self.variables.items():
            snap[n] = (v.value, float(v.belief), dict(v.messages_in))
        return snap

    def _restore(self, snap: Dict[str, Tuple[Any, float, Dict[str, float]]]) -> None:
        for n, (val, bel, msg) in snap.items():
            v = self.variables[n]
            v.value = val
            v.belief = bel
            v.messages_in = dict(msg)

    def infer(self, max_iterations: int = 20, tolerance: float = 0.001) -> Dict[str, Any]:
        """Alternate factor messages and belief means until |ΔVFE| < tolerance or cap."""
        vfe_history: List[float] = []
        last_iter = 0

        for iteration in range(max(1, int(max_iterations))):
            for factor in self.factors.values():
                factor.send_messages()
            for var in self.variables.values():
                var.update_belief()

            vfe = float(self.vfe())
            vfe_history.append(vfe)
            last_iter = iteration + 1

            if len(vfe_history) >= 2:
                delta = abs(vfe_history[-1] - vfe_history[-2])
                if delta < float(tolerance):
                    return {
                        "converged": True,
                        "iterations": last_iter,
                        "vfe": round(vfe, 4),
                        "vfe_history": [round(v, 4) for v in vfe_history],
                        "beliefs": {n: round(float(v.belief), 4) for n, v in self.variables.items()},
                    }

        return {
            "converged": False,
            "iterations": last_iter,
            "vfe": round(float(vfe_history[-1]) if vfe_history else self.vfe(), 4),
            "vfe_history": [round(v, 4) for v in vfe_history],
            "beliefs": {n: round(float(v.belief), 4) for n, v in self.variables.items()},
        }

    def observe(self, variable_name: str, value: Any) -> None:
        """Clamp ``value``; toy certainty sets belief to 0 (deterministic trace readout)."""
        if variable_name in self.variables:
            v = self.variables[variable_name]
            v.value = value
            v.belief = 0.0

    def active_infer(
        self,
        candidate_actions: Sequence[Any],
        action_variable: str,
        *,
        infer_iter: int = 8,
    ) -> Dict[str, Any]:
        """Try each candidate action; restore full graph state between trials."""
        if action_variable not in self.variables:
            return {"error": "unknown action variable", "best_action": None, "best_vfe": 1.0, "all": []}

        base = self._snapshot()
        results: List[Dict[str, Any]] = []

        for action in candidate_actions:
            self._restore(base)
            self.variables[action_variable].value = action
            res = self.infer(max_iterations=infer_iter, tolerance=0.02)
            results.append(
                {
                    "action": action,
                    "expected_vfe": res["vfe"],
                    "converged": res.get("converged", False),
                }
            )

        self._restore(base)
        results.sort(key=lambda x: x["expected_vfe"])
        return {
            "best_action": results[0]["action"] if results else None,
            "best_vfe": results[0]["expected_vfe"] if results else 1.0,
            "all": results,
        }
