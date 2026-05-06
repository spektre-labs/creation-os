# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Prolog-style backward chaining over ground facts and definite clauses.

``Term`` / ``unify`` implement Robinson unification with an **occurs check** (pure Python).
:class:`SigmaSymbolic` performs depth-bounded backward chaining, renames rule variables
(standardise-apart), and records **σ** from :class:`~cos.sigma_gate.SigmaGate` on each
successful **fact** match and each **rule-head** unification. This is a lab neuro-symbolic
hook (training-free KB + shallow proofs), not a full Prolog or SMT backend; see
``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

from typing import Any, Dict, List, Optional, Sequence, Tuple, Union

from cos.sigma_gate import SigmaGate

__all__ = [
    "Fact",
    "Rule",
    "SigmaSymbolic",
    "Term",
    "apply_bindings",
    "occurs_in",
    "unify",
]


def occurs_in(var_name: str, term: "Term") -> bool:
    """True if logical variable ``var_name`` appears in ``term`` (structural)."""
    if term.is_variable:
        return term.name == var_name
    return any(occurs_in(var_name, a) for a in term.args)


def apply_bindings(term: "Term", bindings: Dict[str, "Term"]) -> "Term":
    """Resolve leading variable chain; then recurse into compound terms."""
    cur: Term = term
    seen: set[str] = set()
    while cur.is_variable and cur.name in bindings:
        if cur.name in seen:
            break
        seen.add(cur.name)
        cur = bindings[cur.name]
    if cur.args:
        return Term(cur.name, [apply_bindings(a, bindings) for a in cur.args])
    return Term(cur.name)


class Term:
    """Logical term: nullary constant, variable (name starts with ``?``), or compound ``f(...)``."""

    __slots__ = ("name", "args", "is_variable")

    def __init__(self, name: str, args: Optional[List["Term"]] = None) -> None:
        self.name = str(name)
        self.args = list(args) if args else []
        self.is_variable = self.name.startswith("?")

    def substitute(self, bindings: Dict[str, "Term"]) -> "Term":
        return apply_bindings(self, bindings)

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, Term):
            return NotImplemented
        return self.name == other.name and self.args == other.args

    def __hash__(self) -> int:
        return hash((self.name, tuple(self.args)))

    def __repr__(self) -> str:
        if self.args:
            return f"{self.name}({', '.join(repr(a) for a in self.args)})"
        return self.name


def _as_term(x: Union[str, Term]) -> Term:
    return x if isinstance(x, Term) else Term(str(x))


def _vars_in_term(t: Term) -> List[str]:
    if t.is_variable:
        return [t.name]
    out: List[str] = []
    for a in t.args:
        out.extend(_vars_in_term(a))
    return out


def unify(
    t1: Term,
    t2: Term,
    bindings: Optional[Dict[str, Term]] = None,
) -> Optional[Dict[str, Term]]:
    """Robinson unification with occurs-check; ``bindings`` maps variable *names* to terms."""
    sigma: Dict[str, Term] = dict(bindings or {})
    a1 = apply_bindings(t1, sigma)
    a2 = apply_bindings(t2, sigma)

    if a1 == a2:
        return sigma

    if a1.is_variable:
        if occurs_in(a1.name, a2):
            return None
        sigma[a1.name] = a2
        return sigma

    if a2.is_variable:
        if occurs_in(a2.name, a1):
            return None
        sigma[a2.name] = a1
        return sigma

    if a1.name != a2.name or len(a1.args) != len(a2.args):
        return None

    for x, y in zip(a1.args, a2.args):
        u = unify(x, y, sigma)
        if u is None:
            return None
        sigma = u
    return sigma


class Fact:
    """Ground (or partially structured) fact: ``predicate(arg1, ...)``."""

    def __init__(self, predicate: str, *args: Union[str, Term]) -> None:
        self.term = Term(predicate, [_as_term(a) for a in args])

    def __repr__(self) -> str:
        return repr(self.term)


class Rule:
    """Definite clause: ``head :- body1, body2, ...``."""

    def __init__(self, head: Term, body: List[Term]) -> None:
        self.head = head
        self.body = body

    def __repr__(self) -> str:
        body_str = ", ".join(str(b) for b in self.body)
        return f"{self.head} :- {body_str}"


class SigmaSymbolic:
    """Backward-chaining engine with σ readout per successful fact unification."""

    def __init__(self, gate: Any = None) -> None:
        self.gate = gate or SigmaGate()
        self.facts: List[Fact] = []
        self.rules: List[Rule] = []

    def add_fact(self, predicate: str, *args: Union[str, Term]) -> None:
        self.facts.append(Fact(predicate, *args))

    def add_rule(self, head_pred: str, head_args: Sequence[Union[str, Term]], body: List[Tuple[str, Sequence[Union[str, Term]]]]) -> None:
        """``body`` is ``[(predicate, [arg0, arg1, ...]), ...]``."""
        head = Term(head_pred, [_as_term(a) for a in head_args])
        body_terms = [Term(p, [_as_term(a) for a in args]) for p, args in body]
        self.rules.append(Rule(head, body_terms))

    def query(self, predicate: str, *args: Union[str, Term], max_depth: int = 20) -> Dict[str, Any]:
        """Backward-chaining query; returns binding dicts and an inference trace (σ per fact match)."""
        goal = Term(predicate, [_as_term(a) for a in args])
        ext_vars = frozenset(_vars_in_term(goal))
        solutions: List[Dict[str, Term]] = []
        trace: List[Dict[str, Any]] = []
        self._solve_goal(goal, {}, solutions, trace, 0, max(1, int(max_depth)))
        projected = [{v: apply_bindings(Term(v), sol) for v in ext_vars} for sol in solutions]
        return {"solutions": projected, "trace": trace}

    def _solve_goal(
        self,
        goal: Term,
        bindings: Dict[str, Term],
        solutions: List[Dict[str, Term]],
        trace: List[Dict[str, Any]],
        depth: int,
        max_depth: int,
    ) -> None:
        if depth >= max_depth:
            return
        g = goal.substitute(bindings)

        for fact in self.facts:
            u = unify(g, fact.term, dict(bindings))
            if u is not None:
                σ, verdict = self.gate.score(str(g), str(fact.term))
                trace.append(
                    {
                        "depth": depth,
                        "type": "fact",
                        "goal": str(apply_bindings(g, u)),
                        "matched": str(fact.term),
                        "σ": float(σ),
                        "verdict": str(verdict),
                    }
                )
                solutions.append(u)

        for rule in self.rules:
            fresh = self._freshen(rule, depth)
            u = unify(g, fresh.head, dict(bindings))
            if u is None:
                continue
            σ_h, verdict_h = self.gate.score(str(apply_bindings(g, u)), str(fresh.head))
            trace.append(
                {
                    "depth": depth,
                    "type": "rule",
                    "goal": str(apply_bindings(g, u)),
                    "matched": str(fresh.head),
                    "rule": str(fresh),
                    "σ": float(σ_h),
                    "verdict": str(verdict_h),
                }
            )
            self._solve_goals(fresh.body, u, solutions, trace, depth + 1, max_depth)

    def _solve_goals(
        self,
        body: List[Term],
        bindings: Dict[str, Term],
        solutions: List[Dict[str, Term]],
        trace: List[Dict[str, Any]],
        depth: int,
        max_depth: int,
    ) -> None:
        if depth >= max_depth:
            return
        if not body:
            solutions.append(dict(bindings))
            return
        first, *rest = body
        partials: List[Dict[str, Term]] = []
        self._solve_goal(first, bindings, partials, trace, depth, max_depth)
        for b in partials:
            self._solve_goals(rest, b, solutions, trace, depth + 1, max_depth)

    def _freshen(self, rule: Rule, depth: int) -> Rule:
        """Standardise-apart: one fresh name per logical variable in the rule."""
        mapping: Dict[str, str] = {}
        counter: List[int] = [0]

        def collect(t: Term) -> None:
            if t.is_variable:
                if t.name not in mapping:
                    base = t.name.lstrip("?") or "V"
                    mapping[t.name] = f"?_{base}_{depth}_{counter[0]}"
                    counter[0] += 1
            for a in t.args:
                collect(a)

        collect(rule.head)
        for b in rule.body:
            collect(b)

        def rename(t: Term) -> Term:
            if t.is_variable:
                return Term(mapping[t.name])
            if t.args:
                return Term(t.name, [rename(a) for a in t.args])
            return Term(t.name)

        return Rule(rename(rule.head), [rename(b) for b in rule.body])

    def σ_proof(self, trace: Sequence[Dict[str, Any]]) -> float:
        """Conservative proof stress: maximum σ observed on the trace."""
        if not trace:
            return 1.0
        return max(float(step.get("σ", 0.0)) for step in trace)
