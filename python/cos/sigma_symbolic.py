# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
v184 σ-symbolic: backward chaining + unification + unit resolution (lab).

**Not Full Prolog:** function symbols, cuts, and complete FOL semantics are out of scope.
σ can be depth-based and/or gate-scored on proof traces — see ``inference_sigma``.
"""
from __future__ import annotations

import json
import re
from typing import Any, Dict, Iterator, List, Mapping, Optional, Sequence, Union

Term = Union[str, int, float]
Goal = Dict[str, Any]
Binding = Dict[str, Term]

_GOAL_RE = re.compile(r"^\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*\(([^)]*)\)\s*$")


def _split_top_level_commas(s: str) -> List[str]:
    """Split on commas not inside parentheses (rule body: ``a,b,c`` where each part is a goal)."""
    parts: List[str] = []
    depth = 0
    start = 0
    for i, ch in enumerate(s):
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
        elif ch == "," and depth == 0:
            chunk = s[start:i].strip()
            if chunk:
                parts.append(chunk)
            start = i + 1
    tail = s[start:].strip()
    if tail:
        parts.append(tail)
    return parts


def parse_goal(s: str) -> Goal:
    m = _GOAL_RE.match(str(s).strip())
    if not m:
        raise ValueError(f"bad goal syntax: {s!r}")
    pred = m.group(1)
    inner = m.group(2).strip()
    if not inner:
        args: List[Term] = []
    else:
        args = [a.strip() for a in inner.split(",")]
    return {"pred": pred, "args": args}


def parse_rule(s: str) -> Dict[str, Any]:
    text = str(s).strip()
    if ":-" not in text:
        raise ValueError(f"rule needs ':-': {s!r}")
    head_s, body_s = text.split(":-", 1)
    head = parse_goal(head_s.strip())
    body_part = body_s.strip()
    if not body_part:
        body: List[Goal] = []
    else:
        body = [parse_goal(part.strip()) for part in _split_top_level_commas(body_part)]
    return {"head": head, "body": body}


class SigmaSymbolic:
    def __init__(self, gate: Any) -> None:
        self.gate = gate
        self.facts: List[Goal] = []
        self.rules: List[Dict[str, Any]] = []
        self._rule_rename_seq = 0

    def _rename_rule_vars(self, rule: Mapping[str, Any]) -> Dict[str, Any]:
        """Standardize apart: rule variables must not clash with query variables (Prolog-style)."""
        names: set = set()
        for g in (rule["head"], *rule["body"]):
            for a in g["args"]:
                if self.is_variable(a):
                    names.add(str(a))
        if not names:
            return {"head": dict(rule["head"]), "body": [dict(x) for x in rule["body"]]}
        self._rule_rename_seq += 1
        sid = self._rule_rename_seq
        mp = {n: f"V{sid}_{i}_{n}" for i, n in enumerate(sorted(names))}

        def trm(t: Term) -> Term:
            if self.is_variable(t):
                return mp.get(str(t), t)
            return t

        def gcpy(g: Mapping[str, Any]) -> Goal:
            return {"pred": g["pred"], "args": [trm(a) for a in g["args"]]}

        return {"head": gcpy(rule["head"]), "body": [gcpy(x) for x in rule["body"]]}

    def assert_fact(self, predicate: str, *args: Term) -> None:
        self.facts.append({"pred": str(predicate), "args": [str(a) for a in args]})

    def assert_fact_goal(self, goal: Goal) -> None:
        self.facts.append({"pred": str(goal["pred"]), "args": list(goal["args"])})

    def assert_rule(self, head: Goal, body: Sequence[Goal]) -> None:
        self.rules.append({"head": dict(head), "body": [dict(g) for g in body]})

    def assert_rule_dict(self, rule: Mapping[str, Any]) -> None:
        self.rules.append({"head": dict(rule["head"]), "body": [dict(g) for g in rule["body"]]})

    def query(self, goal: Union[str, Goal], *, max_depth: int = 24) -> List[Dict[str, Any]]:
        g = parse_goal(goal) if isinstance(goal, str) else dict(goal)
        solutions: List[Dict[str, Any]] = []
        for sol in self.solve(g, {}, depth=0, max_depth=int(max_depth)):
            sigma = float(self.inference_sigma(g, sol))
            row = {**sol, "sigma": sigma}
            solutions.append(row)
        solutions.sort(key=lambda s: float(s.get("sigma", 1.0)))
        return solutions

    def solve(self, goal: Goal, bindings: Binding, depth: int, max_depth: int) -> Iterator[Dict[str, Any]]:
        if depth > max_depth:
            return
        pred = goal["pred"]
        args = [self.substitute(a, bindings) for a in goal["args"]]

        for fact in self.facts:
            if fact["pred"] != pred:
                continue
            merged = self.unify(args, list(fact["args"]), dict(bindings))
            if merged is not None:
                yield {"bindings": merged, "via": "fact", "depth": depth}

        for rule in self.rules:
            if rule["head"]["pred"] != pred:
                continue
            fresh = self._rename_rule_vars(rule)
            rb = dict(bindings)
            merged = self.unify(args, [self.substitute(x, rb) for x in fresh["head"]["args"]], rb)
            if merged is None:
                continue
            yield from self.solve_body(fresh["body"], merged, depth, max_depth)

    def solve_body(
        self,
        body: Sequence[Goal],
        bindings: Binding,
        depth: int,
        max_depth: int,
    ) -> Iterator[Dict[str, Any]]:
        if not body:
            yield {"bindings": dict(bindings), "via": "rule", "depth": depth}
            return
        first = body[0]
        rest = list(body[1:])
        for sol in self.solve(first, dict(bindings), depth, max_depth):
            merged: Binding = {**bindings, **sol["bindings"]}
            next_depth = int(sol.get("depth", depth)) + 1
            yield from self.solve_body(rest, merged, next_depth, max_depth)

    def unify(self, a_args: Sequence[Term], b_args: Sequence[Term], bindings: Binding) -> Optional[Binding]:
        if len(a_args) != len(b_args):
            return None
        result: Binding = dict(bindings)
        for a0, b0 in zip(a_args, b_args):
            a = self.substitute(a0, result)
            b = self.substitute(b0, result)
            if a == b:
                continue
            if self.is_variable(a):
                result[str(a)] = b
            elif self.is_variable(b):
                result[str(b)] = a
            else:
                return None
        return result

    def substitute(self, term: Term, bindings: Binding) -> Term:
        if not isinstance(term, str):
            return term
        t = term
        seen = 0
        while self.is_variable(t) and str(t) in bindings and seen < 256:
            t = bindings[str(t)]
            seen += 1
        return t

    @staticmethod
    def is_variable(term: Term) -> bool:
        if not isinstance(term, str) or not term:
            return False
        return term[0].isupper()

    def inference_sigma(self, goal: Goal, solution: Mapping[str, Any]) -> float:
        depth = int(solution.get("depth", 0))
        stress = min(1.0, depth * 0.1)
        try:
            gtxt = json.dumps(goal, ensure_ascii=False, sort_keys=True)
            stxt = json.dumps(
                {"bindings": solution.get("bindings"), "via": solution.get("via")},
                ensure_ascii=False,
                sort_keys=True,
            )
            s, _ = self.gate.score(gtxt, stxt)
            stress = 0.5 * stress + 0.5 * float(s)
        except Exception:
            # Gate unavailable or non-JSON-safe; keep depth-based stress only.
            ...
        return float(stress)

    @staticmethod
    def literal(pred: str, args: Sequence[Term], *, neg: bool = False) -> Dict[str, Any]:
        return {"pred": str(pred), "args": [str(x) for x in args], "neg": bool(neg)}

    def is_complement(self, a: Mapping[str, Any], b: Mapping[str, Any]) -> bool:
        if str(a.get("pred")) != str(b.get("pred")):
            return False
        return bool(a.get("neg")) != bool(b.get("neg"))

    def resolution(
        self,
        clause_a: Sequence[Mapping[str, Any]],
        clause_b: Sequence[Mapping[str, Any]],
    ) -> Optional[Dict[str, Any]]:
        la = list(clause_a)
        lb = list(clause_b)
        for lit_a in la:
            for lit_b in lb:
                if not self.is_complement(lit_a, lit_b):
                    continue
                u = self.unify(list(lit_a["args"]), list(lit_b["args"]), {})
                if u is None:
                    continue
                resolvent: List[Dict[str, Any]] = []
                for lit_i in la:
                    if lit_i is not lit_a:
                        resolvent.append(dict(lit_i))
                for lit_i in lb:
                    if lit_i is not lit_b:
                        resolvent.append(dict(lit_i))
                for lit in resolvent:
                    lit["args"] = [str(self.substitute(x, u)) for x in lit["args"]]
                return {"resolvent": resolvent, "bindings": u}
        return None

    def to_dict(self) -> Dict[str, Any]:
        return {"facts": list(self.facts), "rules": list(self.rules)}

    def from_dict(self, blob: Mapping[str, Any]) -> None:
        facts = blob.get("facts")
        rules = blob.get("rules")
        if isinstance(facts, list):
            self.facts = [dict(x) for x in facts if isinstance(x, dict)]
        if isinstance(rules, list):
            self.rules = [dict(x) for x in rules if isinstance(x, dict)]


__all__ = ["SigmaSymbolic", "parse_goal", "parse_rule"]
