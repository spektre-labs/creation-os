# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Causal reasoning: directed causal edges + σ + DAG + do-calculus + counterfactual (Pearl).

**Three levels (lab mapping):** observe parents/children (*see*), :meth:`CausalGraph.do` (*do*),
:meth:`CausalGraph.counterfactual` (*imagine*). This is not a substitute for full SCM identification;
see ``docs/CLAIM_DISCIPLINE.md``.

:class:`PearlLadder` + :class:`PearlDAG` provide a **second, gate-scored narrative** over Pearl’s
see / do / imagine tallness (lab pedagogy only — not an identification proof)."""
from __future__ import annotations

from collections import deque
from typing import Any, Dict, List, Optional, Set

__all__ = ["CausalEdge", "CausalGraph", "PearlDAG", "PearlLadder"]


def _norm_node(x: str) -> str:
    return str(x).strip().lower()


class CausalEdge:
    """One directed causal link: ``cause`` → ``effect`` with optional strength and gate σ."""

    __slots__ = ("cause", "effect", "strength", "sigma")

    def __init__(
        self,
        cause: str,
        effect: str,
        strength: float = 1.0,
        sigma: float = 0.5,
    ) -> None:
        self.cause = _norm_node(cause)
        self.effect = _norm_node(effect)
        self.strength = float(strength)
        self.sigma = float(sigma)

    def to_dict(self) -> Dict[str, Any]:
        return {
            "cause": self.cause,
            "effect": self.effect,
            "strength": round(self.strength, 4),
            "sigma": round(self.sigma, 4),
            "σ": round(self.sigma, 4),
        }


class CausalGraph:
    """DAG of causal edges. Each edge carries σ from :class:`~cos.sigma_gate.SigmaGate` (or override)."""

    def __init__(self, gate: Any = None) -> None:
        from cos.sigma_gate import SigmaGate

        self.gate = gate or SigmaGate()
        self.edges: List[CausalEdge] = []
        self._adjacency: Dict[str, List[CausalEdge]] = {}
        self._reverse: Dict[str, List[CausalEdge]] = {}

    @classmethod
    def from_sigma_graph(
        cls,
        sigma_graph: Any,
        gate: Any = None,
        *,
        causal_relations: Optional[Set[str]] = None,
    ) -> "CausalGraph":
        """Build a causal DAG from a :class:`~cos.graph.SigmaGraph` (subject → object as cause → effect).

        Only triples whose relation matches ``causal_relations`` (default: names containing ``caus`` or
        ``leads``) are imported; σ is taken from the triple when possible.
        """
        cg = cls(gate=gate or getattr(sigma_graph, "gate", None))
        rel_allow = causal_relations
        for triple in getattr(sigma_graph, "triples", {}).values():
            rel = str(getattr(triple, "relation", "")).strip().lower()
            if rel_allow is not None:
                if rel not in rel_allow:
                    continue
            else:
                if not ("caus" in rel or rel == "leads_to" or "lead" in rel):
                    continue
            sig = getattr(triple, "sigma", None)
            cg.add(
                getattr(triple, "subject", ""),
                getattr(triple, "object", ""),
                strength=1.0,
                sigma=float(sig) if sig is not None else None,
            )
        return cg

    def _would_create_cycle(self, cause: str, effect: str) -> bool:
        """E → … → C would close a loop if we add C → E."""
        c, e = _norm_node(cause), _norm_node(effect)
        if c == e:
            return True
        visited: Set[str] = set()
        frontier: List[str] = [e]
        while frontier:
            cur = frontier.pop()
            if cur == c:
                return True
            if cur in visited:
                continue
            visited.add(cur)
            for edge in self._adjacency.get(cur, []):
                frontier.append(edge.effect)
        return False

    def add(
        self,
        cause: str,
        effect: str,
        strength: float = 1.0,
        *,
        sigma: Optional[float] = None,
    ) -> Dict[str, Any]:
        """Add a causal edge; σ from gate unless ``sigma`` is provided (import / tests)."""
        c, eff = _norm_node(cause), _norm_node(effect)
        if self._would_create_cycle(c, eff):
            return {"added": False, "reason": "cycle", "sigma": 1.0, "verdict": "RETHINK"}
        if sigma is None:
            prompt = f"Does {c} cause {eff}?"
            response = f"{c} causes {eff}"
            sig_f, verdict = self.gate.score(prompt, response)
            s = float(sig_f)
            ver = str(verdict)
        else:
            s = float(sigma)
            ver = "IMPORT"
        edge = CausalEdge(c, eff, strength=strength, sigma=s)
        self.edges.append(edge)
        self._adjacency.setdefault(c, []).append(edge)
        self._reverse.setdefault(eff, []).append(edge)
        return {"added": True, "sigma": s, "verdict": ver}

    def causes_of(self, effect: str) -> List[Dict[str, Any]]:
        """Level 1 (*see*): parents of ``effect``."""
        return [e.to_dict() for e in self._reverse.get(_norm_node(effect), [])]

    def effects_of(self, cause: str) -> List[Dict[str, Any]]:
        """Level 1 (*see*): children of ``cause``."""
        return [e.to_dict() for e in self._adjacency.get(_norm_node(cause), [])]

    def do(self, intervention_var: str, value: Any = None) -> Dict[str, Any]:
        """``do(X=x)`` sketch: list incoming causes severed and downstream descendants (lab)."""
        iv = _norm_node(intervention_var)
        intervened = self._reverse.get(iv, [])
        severed = [e.cause for e in intervened]
        downstream = self._propagate(iv)
        σ_total = self._path_sigma(iv, downstream)
        return {
            "intervention": iv,
            "value": value,
            "severed_causes": severed,
            "downstream_effects": downstream,
            "σ_total": σ_total,
        }

    def counterfactual(
        self,
        observed: Dict[str, Any],
        intervention: Dict[str, Any],
        outcome_var: str,
    ) -> Dict[str, Any]:
        """Level 3 (*imagine*): abduction (stub) + intervention + prediction (stub) + path σ."""
        exogenous = self._abduct(observed)
        modified = {**observed, **intervention}
        result = self._predict_outcome(modified, _norm_node(outcome_var), exogenous)
        iv_keys = list(intervention.keys())
        out_n = _norm_node(outcome_var)
        starters = {_norm_node(str(k)) for k in intervention.keys()} | {
            _norm_node(str(k)) for k in observed.keys()
        }
        path: Optional[List[str]] = None
        for k in starters:
            p = self.find_path(k, out_n)
            if p and (path is None or len(p) > len(path)):
                path = p
        if not path and iv_keys:
            path = self.find_path(_norm_node(str(iv_keys[0])), out_n)
        if path:
            σ_path = self._path_sigma_along(path)
            σ = round(σ_path, 4)
        else:
            σ = 1.0
        return {
            "question": f"What if {intervention}?",
            "observed_outcome": observed.get(outcome_var),
            "counterfactual_outcome": result,
            "σ": σ,
            "path": path,
        }

    def root_cause(self, effect: str, max_depth: int = 5) -> List[Dict[str, Any]]:
        """Backward traces to exogenous nodes; sorted by cumulative σ (lower first)."""
        results: List[Dict[str, Any]] = []
        self._trace_back(_norm_node(effect), [], results, depth=0, max_depth=max_depth)
        results.sort(key=lambda r: float(r["cumulative_σ"]))
        return results

    def find_path(self, start: str, end: str) -> Optional[List[str]]:
        """First path ``start`` → ``end`` along causal edges (DFS, acyclic)."""
        s, t = _norm_node(start), _norm_node(end)

        def dfs(cur: str, path: List[str]) -> Optional[List[str]]:
            if cur == t:
                return path
            for edge in self._adjacency.get(cur, []):
                nxt = edge.effect
                if nxt in path:
                    continue
                hit = dfs(nxt, path + [nxt])
                if hit:
                    return hit
            return None

        return dfs(s, [s])

    def _propagate(self, start: str) -> List[str]:
        out: List[str] = []
        seen: Set[str] = set()
        q: deque[str] = deque([_norm_node(start)])
        while q:
            cur = q.popleft()
            for edge in self._adjacency.get(cur, []):
                eff = edge.effect
                if eff in seen:
                    continue
                seen.add(eff)
                out.append(eff)
                q.append(eff)
        return out

    def _trace_back(
        self,
        node: str,
        acc_edges: List[CausalEdge],
        results: List[Dict[str, Any]],
        depth: int,
        max_depth: int,
    ) -> None:
        if depth > max_depth:
            return
        causes = self._reverse.get(node, [])
        if not causes:
            cum = sum(e.sigma for e in acc_edges)
            path_nodes = [node]
            for e in acc_edges:
                path_nodes.append(e.effect)
            results.append(
                {
                    "root": node,
                    "path": path_nodes,
                    "depth": depth,
                    "cumulative_σ": round(float(cum), 4),
                },
            )
            return
        for edge in causes:
            self._trace_back(edge.cause, acc_edges + [edge], results, depth + 1, max_depth)

    def _path_sigma(self, start: str, downstream: List[str]) -> float:
        if not downstream:
            return 0.0
        total = 0.0
        cur = _norm_node(start)
        for node in downstream:
            step = _norm_node(node)
            found = False
            for edge in self._adjacency.get(cur, []):
                if edge.effect == step:
                    total += edge.sigma
                    found = True
                    break
            if not found:
                total += 1.0
            cur = step
        return round(total / max(len(downstream), 1), 4)

    def _path_sigma_along(self, path: List[str]) -> float:
        if not path or len(path) < 2:
            return 0.0
        total = 0.0
        for i in range(len(path) - 1):
            a, b = path[i], path[i + 1]
            hit = False
            for edge in self._adjacency.get(a, []):
                if edge.effect == b:
                    total += edge.sigma
                    hit = True
                    break
            if not hit:
                total += 1.0
        return round(total / max(len(path) - 1, 1), 4)

    def _abduct(self, observed: Dict[str, Any]) -> Dict[str, Any]:
        return dict(observed)

    def _predict_outcome(
        self,
        state: Dict[str, Any],
        outcome_var: str,
        exogenous: Dict[str, Any],
    ) -> Any:
        del exogenous
        return state.get(outcome_var)


def _verdict_str(v: Any) -> str:
    raw = str(getattr(v, "name", v))
    return raw.split(".")[-1] if "." in raw else raw


class PearlDAG:
    """Lightweight cause → effect adjacency + confounder pairs (for :class:`PearlLadder` demos)."""

    def __init__(self) -> None:
        self.edges: Dict[str, List[Dict[str, Any]]] = {}
        self.confounders: Dict[Any, Any] = {}

    def add(self, cause: str, effect: str, σ: Optional[float] = None) -> None:
        c, e = str(cause).strip(), str(effect).strip()
        self.edges.setdefault(c, []).append({"effect": e, "σ": σ})

    def add_confounder(self, var_a: str, var_b: str, confounder: str) -> None:
        a, b = str(var_a).strip(), str(var_b).strip()
        cf = str(confounder).strip()
        self.confounders[(a, b)] = cf
        self.confounders[(b, a)] = cf

    def effects_of(self, cause: str) -> List[Dict[str, Any]]:
        return list(self.edges.get(str(cause).strip(), []))

    def causes_of(self, effect: str) -> List[Dict[str, Any]]:
        eff = str(effect).strip()
        causes: List[Dict[str, Any]] = []
        for cause, outs in self.edges.items():
            for row in outs:
                if row.get("effect") == eff:
                    causes.append({"cause": cause, "σ": row.get("σ")})
        return causes

    def has_confounder(self, a: str, b: str) -> bool:
        return (str(a).strip(), str(b).strip()) in self.confounders


class PearlLadder:
    """Pearl-style SEE / DO / IMAGINE hooks scored only via :class:`~cos.sigma_gate.SigmaGate`."""

    def __init__(self, gate: Any = None, graph: Optional[PearlDAG] = None) -> None:
        from cos.sigma_gate import SigmaGate

        self.gate = gate or SigmaGate()
        self.graph = graph or PearlDAG()

    def see(self, x: str, y: str) -> Dict[str, Any]:
        σ, verdict = self.gate.score(f"observe {x}", f"correlates with {y}")
        confounded = self.graph.has_confounder(x, y)
        cf_name = self.graph.confounders.get((str(x).strip(), str(y).strip()))
        return {
            "rung": 1,
            "query": f"P({y}|{x})",
            "type": "association",
            "σ": round(float(σ), 4),
            "σ_see": round(float(σ), 4),
            "verdict": _verdict_str(verdict),
            "confounded": confounded,
            "warning": (f"Confounded by {cf_name}" if confounded else None),
            "interpretation": (
                f"Observing {x} correlates with {y} (association only)"
                if not confounded
                else f"Association between {x} and {y} may be confounded — do not infer effect"
            ),
        }

    def do(self, x: str, y: str) -> Dict[str, Any]:
        σ_see = float(self.see(x, y)["σ"])
        σ_do_raw, verdict = self.gate.score(f"intervene: set {x}", f"effect on {y}")
        σ_do = float(σ_do_raw)
        confounded = self.graph.has_confounder(x, y)
        if confounded:
            σ_do = min(σ_do + 0.1, 1.0)
        return {
            "rung": 2,
            "query": f"P({y}|do({x}))",
            "type": "intervention",
            "σ_see": round(σ_see, 4),
            "σ_do": round(σ_do, 4),
            "verdict": _verdict_str(verdict),
            "causal": True,
            "confounded": confounded,
            "do_calculus_applied": confounded,
            "interpretation": f"If we SET {x}, stress on effect {y} is summarized by σ_do≈{σ_do:.3f} (lab)",
        }

    def imagine(self, x_actual: str, y_actual: str, x_counterfactual: str) -> Dict[str, Any]:
        σ_see = float(self.see(x_actual, y_actual)["σ"])
        σ_do = float(self.do(x_counterfactual, y_actual)["σ_do"])
        σ_cf_raw, verdict = self.gate.score(
            f"given {x_actual}→{y_actual}, what if {x_counterfactual}?",
            f"counterfactual {y_actual}",
        )
        σ_cf = max(float(σ_cf_raw), σ_do)
        return {
            "rung": 3,
            "query": f"P({y_actual}_{x_counterfactual}|{x_actual},{y_actual})",
            "type": "counterfactual",
            "σ_see": round(σ_see, 4),
            "σ_do": round(σ_do, 4),
            "σ_imagine": round(σ_cf, 4),
            "verdict": _verdict_str(verdict),
            "interpretation": (
                f"Counterfactual stress (vs do-bound) σ_imagine≈{σ_cf:.3f} — harder than association alone (lab)"
            ),
        }

    def root_cause(self, effect: str, max_depth: int = 5) -> Dict[str, Any]:
        path: List[Dict[str, Any]] = []
        current = str(effect).strip()
        visited: Set[str] = set()

        for _ in range(int(max_depth)):
            if current in visited:
                break
            visited.add(current)
            causes = self.graph.causes_of(current)
            if not causes:
                break
            best = min(causes, key=lambda c: float(c.get("σ") if c.get("σ") is not None else 1.0))
            path.append({"from": best["cause"], "to": current, "σ": best.get("σ")})
            current = str(best["cause"])

        return {
            "effect": effect,
            "root_cause": current,
            "path": path,
            "depth": len(path),
        }

    def ladder_summary(self, x: str, y: str) -> Dict[str, Any]:
        return {
            "L1_see": self.see(x, y),
            "L2_do": self.do(x, y),
            "L3_imagine": self.imagine(x, y, f"not_{x}"),
            "note": "σ typically rises from see→do→imagine in this lab sketch; bind claims to harness or SCM ID.",
        }
