# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Neuro-symbolic ``σ``-reasoning: FOL clauses, Robinson unification, binary resolution.

LLM (or parser) supplies facts and clauses; this module checks consistency and
refutation proofs **without** external SMT or Prover9 — a minimal educational core.

Related lines of work (citations are positioning only; this file does not embed
external provers): LLM + first-order backends, backward chaining with SLD-style
search, DSL-to-solver pipelines, and unsat-core-style verbalization.
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Dict, List, Optional, Set, Tuple, Union

Subst = Dict[str, "Term"]


class Term:
    """FOL term: variable, constant, or function application."""

    pass


@dataclass(frozen=True, eq=True)
class Var(Term):
    name: str

    def __repr__(self) -> str:
        return f"?{self.name}"


@dataclass(frozen=True, eq=True)
class Const(Term):
    name: str

    def __repr__(self) -> str:
        return self.name


@dataclass(frozen=True, eq=True)
class Func(Term):
    name: str
    args: Tuple[Term, ...]

    def __init__(self, name: str, args: Union[List[Term], Tuple[Term, ...]]) -> None:
        object.__setattr__(self, "name", name)
        object.__setattr__(self, "args", tuple(args))

    def __repr__(self) -> str:
        inner = ",".join(str(a) for a in self.args)
        return f"{self.name}({inner})"


@dataclass(frozen=True, eq=True)
class Predicate:
    name: str
    args: Tuple[Term, ...]
    negated: bool = False

    def __init__(
        self,
        name: str,
        args: Union[List[Term], Tuple[Term, ...]],
        negated: bool = False,
    ) -> None:
        object.__setattr__(self, "name", name)
        object.__setattr__(self, "args", tuple(args))
        object.__setattr__(self, "negated", negated)

    def negate(self) -> Predicate:
        return Predicate(self.name, list(self.args), not self.negated)

    def __repr__(self) -> str:
        sign = "¬" if self.negated else ""
        inner = ",".join(str(a) for a in self.args)
        return f"{sign}{self.name}({inner})"


@dataclass(frozen=True, eq=True)
class Clause:
    literals: frozenset[Predicate]

    def __init__(self, literals: List[Predicate]) -> None:
        object.__setattr__(self, "literals", frozenset(literals))

    def __repr__(self) -> str:
        return " ∨ ".join(str(lit) for lit in sorted(self.literals, key=repr))

    def __len__(self) -> int:
        return len(self.literals)

    def is_empty(self) -> bool:
        return len(self.literals) == 0


def occurs_check(var: Var, term: Term) -> bool:
    if var == term:
        return True
    if isinstance(term, Func):
        return any(occurs_check(var, a) for a in term.args)
    return False


def apply_subst(term: Term, subst: Subst) -> Term:
    if isinstance(term, Var):
        if term.name in subst:
            return apply_subst(subst[term.name], subst)
        return term
    if isinstance(term, Func):
        return Func(term.name, [apply_subst(a, subst) for a in term.args])
    return term


def apply_subst_pred(pred: Predicate, subst: Subst) -> Predicate:
    return Predicate(pred.name, [apply_subst(a, subst) for a in pred.args], pred.negated)


def unify(t1: Term, t2: Term, subst: Optional[Subst] = None) -> Optional[Subst]:
    """Robinson unification; ``subst`` maps variable *names* to terms."""
    if subst is None:
        subst = {}
    subst = dict(subst)

    stack: List[Tuple[Term, Term]] = [(t1, t2)]
    while stack:
        a, b = stack.pop()
        a = apply_subst(a, subst)
        b = apply_subst(b, subst)
        if a == b:
            continue
        if isinstance(a, Var):
            if occurs_check(a, b):
                return None
            subst[a.name] = b
            continue
        if isinstance(b, Var):
            if occurs_check(b, a):
                return None
            subst[b.name] = a
            continue
        if isinstance(a, Func) and isinstance(b, Func):
            if a.name != b.name or len(a.args) != len(b.args):
                return None
            for x, y in zip(a.args, b.args):
                stack.append((x, y))
            continue
        return None
    return subst


def unify_predicates(
    p1: Predicate,
    p2: Predicate,
    subst: Optional[Subst] = None,
) -> Optional[Subst]:
    if p1.name != p2.name or len(p1.args) != len(p2.args):
        return None
    atom1 = Predicate(p1.name, list(p1.args), negated=False)
    atom2 = Predicate(p2.name, list(p2.args), negated=False)
    subst = dict(subst or {})
    for t_a, t_b in zip(atom1.args, atom2.args):
        u = unify(t_a, t_b, subst)
        if u is None:
            return None
        subst = u
    return subst


def resolve(c1: Clause, c2: Clause) -> List[Clause]:
    """Binary resolution on complementary literals; returns new resolvents."""
    resolvents: List[Clause] = []
    for l1 in c1.literals:
        for l2 in c2.literals:
            if l1.name != l2.name or l1.negated == l2.negated:
                continue
            subst = unify_predicates(
                Predicate(l1.name, list(l1.args), False),
                Predicate(l2.name, list(l2.args), False),
            )
            if subst is None:
                continue
            remaining: Set[Predicate] = set()
            for lit in c1.literals:
                if lit is not l1:
                    remaining.add(apply_subst_pred(lit, subst))
            for lit in c2.literals:
                if lit is not l2:
                    remaining.add(apply_subst_pred(lit, subst))
            resolvents.append(Clause(list(remaining)))
    return resolvents


def prove_by_refutation(
    clauses: List[Clause],
    goal: Predicate,
    max_steps: int = 100,
) -> Dict[str, Any]:
    """Prove ``goal`` by refutation: add ``¬goal`` and search for the empty clause.

    Returns a dict with ``proved``, ``steps``, and ``trace`` (short strings).
    """
    neg_goal = Clause([goal.negate()])
    all_clauses: List[Clause] = list(clauses) + [neg_goal]
    known: Set[frozenset[Predicate]] = {c.literals for c in all_clauses}
    trace: List[str] = []

    for step in range(max_steps):
        new_batch: List[Clause] = []
        n = len(all_clauses)
        for i in range(n):
            for j in range(i + 1, n):
                for r in resolve(all_clauses[i], all_clauses[j]):
                    if r.is_empty():
                        trace.append(
                            f"step {step}: □ from {all_clauses[i]!r} ∧ {all_clauses[j]!r}"
                        )
                        return {"proved": True, "steps": step + 1, "trace": trace}
                    if r.literals not in known:
                        known.add(r.literals)
                        new_batch.append(r)
                        trace.append(
                            f"step {step}: {r!r} from {all_clauses[i]!r} ∧ {all_clauses[j]!r}"
                        )
        if not new_batch:
            break
        all_clauses.extend(new_batch)

    return {"proved": False, "steps": max_steps, "trace": trace}


class SigmaReason:
    """Consistency and refutation checks; ``sigma`` rises with detected conflicts."""

    def check_consistency(
        self,
        facts: List[Tuple[str, List[str]]],
        uniqueness: Optional[List[str]] = None,
    ) -> Dict[str, Any]:
        """Check a flat fact list for uniqueness violations and direct ``P`` / ``not_P`` clashes."""
        uniqueness = uniqueness or []
        conflicts: List[Dict[str, Any]] = []

        for pred_name in uniqueness:
            pred_facts = [f for f in facts if f[0] == pred_name]
            groups: Dict[str, List[List[str]]] = {}
            for _, args in pred_facts:
                key = args[0] if args else ""
                groups.setdefault(key, []).append(args[1:] if len(args) > 1 else [])

            for key, values in groups.items():
                if len(values) > 1:
                    conflicts.append(
                        {
                            "predicate": pred_name,
                            "subject": key,
                            "values": values,
                            "type": "uniqueness_violation",
                        }
                    )

        pos_facts: Set[Tuple[str, Tuple[str, ...]]] = set()
        neg_facts: Set[Tuple[str, Tuple[str, ...]]] = set()
        for pred_name, args in facts:
            tup = tuple(args)
            if pred_name.startswith("not_"):
                neg_facts.add((pred_name[4:], tup))
            else:
                pos_facts.add((pred_name, tup))

        for dc in pos_facts & neg_facts:
            conflicts.append(
                {
                    "predicate": dc[0],
                    "args": list(dc[1]),
                    "type": "direct_contradiction",
                }
            )

        n_facts = len(facts)
        n_conflicts = len(conflicts)
        if n_facts == 0:
            sigma = 0.5
        else:
            sigma = min(1.0, float(n_conflicts) / float(max(n_facts, 1)))

        consistent = n_conflicts == 0
        return {
            "consistent": consistent,
            "sigma": sigma,
            "verdict": "ACCEPT" if consistent else "ABSTAIN",
            "conflicts": conflicts,
            "n_facts": n_facts,
            "n_conflicts": n_conflicts,
        }

    def verify_claim(self, knowledge_base: List[Clause], claim: Predicate) -> Dict[str, Any]:
        """Try to derive ``claim`` from ``knowledge_base`` by propositional resolution."""
        result = prove_by_refutation(knowledge_base, claim)
        if result["proved"]:
            return {
                "sigma": 0.05,
                "verdict": "ACCEPT",
                "proved": True,
                "steps": result["steps"],
                "trace": result["trace"],
            }
        return {
            "sigma": 0.7,
            "verdict": "RETHINK",
            "proved": False,
            "steps": result["steps"],
            "trace": result["trace"],
        }


__all__ = [
    "Clause",
    "Const",
    "Func",
    "Predicate",
    "SigmaReason",
    "Term",
    "Var",
    "apply_subst",
    "apply_subst_pred",
    "occurs_check",
    "prove_by_refutation",
    "resolve",
    "unify",
    "unify_predicates",
]
