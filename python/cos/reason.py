# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Neuro-symbolic ``σ``-reasoning: FOL clauses, Robinson unification, binary resolution.

:class:`SigmaReason` also exposes lightweight **triplet KB** helpers (``parse`` / ``unify`` /
``resolve``) for graph-aligned queries with σ per match. Full clause machinery uses
``Predicate`` / ``Clause``; this module checks consistency and refutation proofs **without**
external SMT or Prover9 — a minimal educational core.

Related lines of work (citations are positioning only; this file does not embed
external provers): LLM + first-order backends, backward chaining with SLD-style
search, DSL-to-solver pipelines, and unsat-core-style verbalization.
"""
from __future__ import annotations

import re
from dataclasses import dataclass
from typing import Any, Dict, List, Optional, Set, Tuple, Union

Subst = Dict[str, "Term"]


class Term:
    """FOL term: variable, constant, or function application."""

    __slots__ = ()


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


_LIT_RE = re.compile(
    r"^(?P<neg>(?:¬|not\s+|NOT\s+))?(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*"
    r"\(\s*(?P<args>[^)]*)\)\s*$",
)


def _parse_literal(raw: str) -> Predicate:
    s = raw.strip()
    m = _LIT_RE.match(s)
    if not m:
        raise ValueError(f"cannot parse literal: {raw!r}")
    neg = bool(m.group("neg"))
    name = str(m.group("name"))
    args_raw = (m.group("args") or "").strip()
    args: List[Term] = []
    if args_raw:
        for part in args_raw.split(","):
            part = part.strip()
            if not part:
                continue
            if part.startswith("?"):
                args.append(Var(part[1:].strip()))
            else:
                args.append(Const(part))
    return Predicate(name, args, negated=neg)


def fol_parse(text: str) -> List[Clause]:
    """Parse one clause per line; OR is ``|`` between literals.

    Literals: ``P(a)``, ``¬Q(?x)``, ``not R(b,c)``. Variables use a ``?`` prefix.
    """
    clauses: List[Clause] = []
    for raw_line in str(text).splitlines():
        line = raw_line.split("//")[0].strip()
        if not line:
            continue
        parts = [p.strip() for p in line.split("|")]
        lits = [_parse_literal(p) for p in parts if p]
        if lits:
            clauses.append(Clause(lits))
    return clauses


def prove_by_refutation_with_sigma(
    clauses: List[Clause],
    goal: Predicate,
    gate: Any,
    max_steps: int = 100,
) -> Dict[str, Any]:
    """Resolution refutation with a σ readout per derived clause (via ``gate.score``)."""
    neg_goal = Clause([goal.negate()])
    all_clauses: List[Clause] = list(clauses) + [neg_goal]
    known: Set[frozenset[Predicate]] = {c.literals for c in all_clauses}
    trace: List[str] = []
    step_sigmas: List[float] = []

    def _score(msg: str) -> None:
        s, _ = gate.score("fol_resolution_step", msg[:1500])
        step_sigmas.append(round(float(s), 4))

    for step in range(max_steps):
        new_batch: List[Clause] = []
        n = len(all_clauses)
        for i in range(n):
            for j in range(i + 1, n):
                for r in resolve(all_clauses[i], all_clauses[j]):
                    if r.is_empty():
                        msg = f"step {step}: □ from {all_clauses[i]!r} ∧ {all_clauses[j]!r}"
                        trace.append(msg)
                        _score(msg)
                        return {
                            "proved": True,
                            "steps": step + 1,
                            "trace": trace,
                            "step_sigmas": step_sigmas,
                        }
                    if r.literals not in known:
                        known.add(r.literals)
                        new_batch.append(r)
                        msg = f"step {step}: {r!r} from {all_clauses[i]!r} ∧ {all_clauses[j]!r}"
                        trace.append(msg)
                        _score(msg)
        if not new_batch:
            break
        all_clauses.extend(new_batch)

    return {"proved": False, "steps": max_steps, "trace": trace, "step_sigmas": step_sigmas}


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

    def resolution_with_sigma(
        self,
        knowledge_base: List[Clause],
        claim: Predicate,
        gate: Any,
        *,
        max_steps: int = 100,
    ) -> Dict[str, Any]:
        """Refutation search with σ attached to each resolution trace line."""
        return prove_by_refutation_with_sigma(knowledge_base, claim, gate, max_steps=max_steps)

    def parse(self, statement: str) -> Optional[Tuple[str, str, str]]:
        """Parse ``A rel B-phrase`` → ``(A, rel, B-phrase)`` (whitespace/token heuristic)."""
        parts = statement.strip().split()
        if len(parts) >= 3:
            return (parts[0], parts[1], " ".join(parts[2:]))
        return None

    def unify(
        self,
        term_a: Any,
        term_b: Any,
        substitution: Optional[Dict[str, str]] = None,
    ) -> Optional[Dict[str, str]]:
        """FOL-style unification for strings (``?var``) and nested tuples (lab)."""
        return self._unify_terms(term_a, term_b, dict(substitution or {}))

    def _fol_apply(self, term: Any, sub: Dict[str, str]) -> Any:
        if isinstance(term, str) and term.startswith("?") and term in sub:
            return self._fol_apply(sub[term], sub)
        if isinstance(term, tuple):
            return tuple(self._fol_apply(x, sub) for x in term)
        return term

    def _unify_terms(self, term_a: Any, term_b: Any, sub: Dict[str, str]) -> Optional[Dict[str, str]]:
        term_a = self._fol_apply(term_a, sub)
        term_b = self._fol_apply(term_b, sub)
        if term_a == term_b:
            return sub
        if isinstance(term_a, str) and term_a.startswith("?"):
            if term_a in sub:
                return self._unify_terms(sub[term_a], term_b, sub)
            out = dict(sub)
            out[term_a] = str(term_b)
            return out
        if isinstance(term_b, str) and term_b.startswith("?"):
            if term_b in sub:
                return self._unify_terms(term_a, sub[term_b], sub)
            out = dict(sub)
            out[term_b] = str(term_a)
            return out
        if isinstance(term_a, tuple) and isinstance(term_b, tuple):
            if len(term_a) != len(term_b):
                return None
            cur = dict(sub)
            for x, y in zip(term_a, term_b):
                nxt = self._unify_terms(x, y, cur)
                if nxt is None:
                    return None
                cur = nxt
            return cur
        return None

    def resolve(
        self,
        knowledge_base: List[Tuple[str, str, str]],
        query: Tuple[str, str, str],
        gate: Any,
        *,
        max_depth: int = 10,
    ) -> Dict[str, Any]:
        """Backward-style KB lookup with σ per attempted match; verdict from max step σ."""
        steps: List[Dict[str, Any]] = []
        result = self._resolve_recursive(knowledge_base, query, gate, steps, 0, int(max_depth))
        total_σ = max((float(s["σ"]) for s in steps), default=0.0)
        ta = float(getattr(gate, "threshold_accept", 0.15))
        tb = float(getattr(gate, "threshold_abstain", 0.85))
        if total_σ < ta:
            verdict = "ACCEPT"
        elif total_σ > tb:
            verdict = "ABSTAIN"
        else:
            verdict = "RETHINK"
        return {
            "proven": result,
            "steps": steps,
            "total_σ": round(total_σ, 4),
            "total_sigma": round(total_σ, 4),
            "verdict": verdict,
        }

    def _resolve_recursive(
        self,
        kb: List[Tuple[str, str, str]],
        query: Tuple[str, str, str],
        gate: Any,
        steps: List[Dict[str, Any]],
        depth: int,
        max_depth: int,
    ) -> bool:
        if depth >= max_depth:
            return False
        for fact in kb:
            u = self._unify_triple_pattern(query, fact)
            if u is not None:
                σ, _ = gate.score(str(query), str(fact))
                steps.append(
                    {
                        "depth": depth,
                        "query": str(query),
                        "matched": str(fact),
                        "σ": float(σ),
                    }
                )
                return True
        return False

    def _unify_triple_pattern(
        self,
        pattern: Tuple[str, str, str],
        fact: Tuple[str, str, str],
    ) -> Optional[Dict[str, str]]:
        return self._unify_terms(pattern, fact, {})


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
    "fol_parse",
    "occurs_check",
    "prove_by_refutation",
    "prove_by_refutation_with_sigma",
    "resolve",
    "unify",
    "unify_predicates",
]
