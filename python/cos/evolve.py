# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-evolve v2 — lab loop for propose / evaluate / σ-gate / rollback.

``SigmaEvolve.evolve_safe`` adds a **lab** cumulative-risk budget over σ-margin
proposals (bounded-risk scaffolding; not a proof of external theorems).

This is **not** recursive self-improvement in the strong sense and **not** an AGI claim.
Each accepted change is gated by σ movement and optional safety caps. See
``docs/CLAIM_DISCIPLINE.md`` and README evidence ladder: **NOT AGI ACHIEVED**."""
from __future__ import annotations

import copy
import hashlib
import json
import time
from typing import Any, Callable, Dict, List, Optional, Sequence, Tuple, Union

__all__ = ["SigmaAdapter", "SigmaEvolve"]

MutationTarget = str

EvalRow = Union[Tuple[Any, ...], List[Any]]


class SigmaAdapter:
    """GEPA-style adapter: evaluate prompt–response rows and expose trace rows for reflection.

    Decouples the optimizer loop from the concrete scorer; traces carry enough context for
    :meth:`SigmaEvolve.mutate_reflective` without tying to a single object shape."""

    def evaluate(self, gate: Any, test_data: Sequence[EvalRow]) -> List[Dict[str, Any]]:
        results: List[Dict[str, Any]] = []
        for row in test_data:
            if not isinstance(row, (list, tuple)):
                continue
            items = list(row)
            if len(items) < 2:
                continue
            prompt, response = items[0], items[1]
            gold: Any = items[2] if len(items) > 2 else None
            comp = "gate"
            if len(items) > 3 and isinstance(items[3], str) and items[3]:
                comp = str(items[3])
            score_fn = getattr(gate, "score", None)
            if not callable(score_fn):
                continue
            σ, verdict = score_fn(str(prompt), str(response))
            results.append(
                {
                    "σ": float(σ),
                    "verdict": verdict,
                    "correct": gold,
                    "component": comp,
                    "prompt": str(prompt)[:200],
                    "response": str(response)[:200],
                },
            )
        return results

    def extract_traces(self, results: Sequence[Dict[str, Any]]) -> List[Dict[str, Any]]:
        traces: List[Dict[str, Any]] = []
        for r in results:
            traces.append(
                {
                    "component": str(r.get("component", "gate")),
                    "σ": float(r.get("σ", r.get("sigma", 0.0))),
                    "correct": r.get("correct"),
                    "verdict": r.get("verdict"),
                },
            )
        return traces


class SigmaEvolve:
    """Mutation → eval → σ before/after → accept if Δσ ≤ 0 (lower stress) or rollback."""

    MUTATION_TARGETS: tuple[MutationTarget, ...] = (
        "prompts",
        "tools",
        "thresholds",
        "routing",
        "code",
    )

    def __init__(self, gate: Any = None) -> None:
        from cos.sigma_gate import SigmaGate

        self.gate = gate or SigmaGate()
        self.improvement_archive: List[Dict[str, Any]] = []
        self.audit_log: List[Dict[str, Any]] = []
        self.domain_sigma_ceiling: Dict[str, float] = {}
        self.history: List[Dict[str, Any]] = []
        self._run_cost_accum: float = 0.0

    def set_safety_constraint(self, domain: str, max_sigma: float) -> None:
        """Do not promote changes that push domain stress above ``max_sigma`` (lab hook)."""
        self.domain_sigma_ceiling[str(domain)] = float(max_sigma)

    def _system_blob(self, system: Any) -> str:
        try:
            return json.dumps(system, sort_keys=True, default=str)[:8000]
        except TypeError:
            return str(system)[:8000]

    def improve_loop(
        self,
        system: Dict[str, Any],
        eval_fn: Callable[[Dict[str, Any]], float],
        gate: Optional[Any] = None,
        *,
        max_iters: int = 4,
        target: str = "thresholds",
    ) -> Dict[str, Any]:
        g = gate or self.gate
        cur = copy.deepcopy(system)
        out_iters: List[Dict[str, Any]] = []
        for i in range(max(1, int(max_iters))):
            sigma_before = float(g.compute_sigma(None, None, "evolve_system", self._system_blob(cur)))
            proposal = self._mutate(cur, target)
            sigma_after = float(
                g.compute_sigma(None, None, "evolve_system", self._system_blob(proposal)),
            )
            delta = round(sigma_after - sigma_before, 6)
            eval_score = float(eval_fn(proposal))
            safe = self._safety_ok("default", sigma_after)
            accepted = bool(delta <= 0 and safe)
            entry = {
                "iter": i,
                "target": target,
                "sigma_before": sigma_before,
                "sigma_after": sigma_after,
                "delta_sigma": delta,
                "eval_score": eval_score,
                "accepted": accepted,
            }
            self.audit_log.append(dict(entry))
            if accepted:
                cur = proposal
                self.improvement_archive.append(dict(entry))
            out_iters.append(entry)
        return {
            "system": cur,
            "iters": out_iters,
            "n_accepted": sum(1 for x in out_iters if x["accepted"]),
        }

    def meta_evolve_probe(
        self,
        eval_change_description: str,
        gate: Optional[Any] = None,
    ) -> Dict[str, Any]:
        """Score a proposed change to the evaluator itself (self-deception guard lab)."""
        g = gate or self.gate
        sigma = float(g.compute_sigma(None, None, "meta_eval_change", eval_change_description[:2000]))
        verdict = str(g._verdict(sigma))
        return {
            "sigma": sigma,
            "verdict": verdict,
            "blocked": verdict != "ACCEPT" or sigma > g.threshold_abstain,
        }

    def governance_snapshot(self) -> Dict[str, Any]:
        return {
            "audit_entries": len(self.audit_log),
            "archive_entries": len(self.improvement_archive),
            "domains": dict(self.domain_sigma_ceiling),
        }

    def _safety_ok(self, domain: str, sigma_after: float) -> bool:
        cap = self.domain_sigma_ceiling.get(domain)
        if cap is None:
            return True
        return float(sigma_after) <= float(cap)

    def _mutate(self, system: Dict[str, Any], target: str) -> Dict[str, Any]:
        nxt = copy.deepcopy(system)
        nxt.setdefault("thresholds", {})
        nxt.setdefault("prompts", {})
        nxt.setdefault("routing", {})
        if target == "thresholds":
            t = float(nxt["thresholds"].get("tau", 0.3))
            nxt["thresholds"]["tau"] = round(min(0.9, t + 0.02), 4)
        elif target == "prompts":
            nxt["prompts"]["suffix"] = str(nxt["prompts"].get("suffix", "")) + "·"
        elif target == "routing":
            nxt["routing"]["depth"] = int(nxt["routing"].get("depth", 1)) + 1
        else:
            nxt.setdefault("code", {})
            nxt["code"]["rev"] = int(nxt["code"].get("rev", 0)) + 1
        nxt["_mut_ts"] = time.time()
        return nxt

    def mutate_and_verify(
        self,
        system: Dict[str, Any],
        target: str,
        eval_fn: Callable[[Dict[str, Any]], float],
        gate: Optional[Any] = None,
    ) -> Dict[str, Any]:
        """Single mutation with SHA256 evidence anchoring and σ before/after (audit-friendly)."""
        g = gate or self.gate
        blob_pre = self._system_blob(system)
        pre_sha = hashlib.sha256(blob_pre.encode("utf-8")).hexdigest()[:24]
        sigma_before = float(g.compute_sigma(None, None, "evolve_system", blob_pre))
        proposal = self._mutate(system, target)
        blob_post = self._system_blob(proposal)
        post_sha = hashlib.sha256(blob_post.encode("utf-8")).hexdigest()[:24]
        sigma_after = float(g.compute_sigma(None, None, "evolve_system", blob_post))
        delta = round(sigma_after - sigma_before, 6)
        eval_score = float(eval_fn(proposal))
        safe = self._safety_ok("default", sigma_after)
        accepted = bool(delta <= 0 and safe)
        evidence: Dict[str, Any] = {
            "pre_sha256_24": pre_sha,
            "post_sha256_24": post_sha,
            "sigma_before": sigma_before,
            "sigma_after": sigma_after,
            "delta_sigma": delta,
            "eval_score": eval_score,
            "accepted": accepted,
            "target": target,
            "ts": time.time(),
        }
        self.audit_log.append(dict(evidence))
        return {"system": proposal if accepted else system, "evidence": evidence}

    def _rsi_evaluate(self, gate: Any, eval_data: List[Any]) -> float:
        """Margin stress vs ``threshold_accept`` (same σ, looser band ⇒ lower score)."""
        if not eval_data:
            return 0.0
        total = 0.0
        for row in eval_data:
            if not isinstance(row, (list, tuple)) or len(row) < 2:
                continue
            p, r = row[0], row[1]
            s, _ = gate.score(str(p), str(r))
            total += max(0.0, float(s) - float(gate.threshold_accept))
        return float(total) / float(len(eval_data))

    def _rsi_mutate(self, gate: Any, *, sign: float = 1.0) -> Any:
        """Clone thresholds with a bounded nudge (Python-only; no kernel mutation)."""
        from cos.sigma_gate import SigmaGate

        cand = SigmaGate(
            threshold_accept=float(gate.threshold_accept),
            threshold_abstain=float(gate.threshold_abstain),
        )
        step = 0.05 * float(sign)
        cand.threshold_accept = max(0.01, min(0.92, cand.threshold_accept + step))
        if cand.threshold_accept >= cand.threshold_abstain:
            cand.threshold_accept = max(0.01, float(cand.threshold_abstain) - 0.02)
        return cand

    def step(
        self,
        gate: Any,
        eval_data: List[Any],
        formal: Any = None,
        *,
        mutate_sign: float = 1.0,
    ) -> Dict[str, Any]:
        """Single RSI-style propose / σ-margin evaluate / optional formal check (lab)."""
        baseline = float(self._rsi_evaluate(gate, eval_data))
        candidate = self._rsi_mutate(gate, sign=mutate_sign)
        candidate_sigma = float(self._rsi_evaluate(candidate, eval_data))
        if formal is not None:
            if not bool(formal.check_invariants(candidate)):
                return {
                    "accepted": False,
                    "reason": "invariant violation",
                    "σ_before": baseline,
                    "σ_after": baseline,
                }
        if candidate_sigma < baseline:
            return {
                "accepted": True,
                "σ_before": baseline,
                "σ_after": candidate_sigma,
                "improvement": baseline - candidate_sigma,
                "candidate": candidate,
            }
        return {
            "accepted": False,
            "reason": "no improvement",
            "σ_before": baseline,
            "σ_after": baseline,
        }

    def _targeted_mutation(
        self,
        component: str,
        sigma: float,
        base_rules: Optional[Dict[str, Any]] = None,
    ) -> Dict[str, Any]:
        """GEPA-style targeted rule nudge from the worst-scoring trace component (not random)."""
        merged = dict(base_rules or {})
        s = float(sigma)
        c = str(component)
        strict = float(merged.get("prompt_strictness", 0.5))
        if c == "latent":
            merged["routing_depth_bias"] = int(merged.get("routing_depth_bias", 0)) + 1
        elif c == "gate" and s > 0.5:
            merged["prompt_strictness"] = min(1.0, strict + 0.05)
            tn = float(merged.get("threshold_nudge", 0.02))
            merged["threshold_nudge"] = max(0.005, round(tn - 0.005, 4))
        elif c == "gate":
            merged["prompt_strictness"] = max(0.05, round(strict - 0.02, 4))
        return merged

    def mutate_reflective(
        self,
        gate: Any,
        eval_data: List[Any],
        trace: List[Dict[str, Any]],
        *,
        base_rules: Optional[Dict[str, Any]] = None,
    ) -> Dict[str, Any]:
        """Mutation informed by the prior iteration trace — prioritize the highest-σ component."""
        del gate, eval_data
        if not trace:
            return dict(base_rules or {})
        worst = max(trace, key=lambda t: float(t.get("σ", t.get("sigma", 0.0))))
        comp = str(worst.get("component", "gate"))
        sig = float(worst.get("σ", worst.get("sigma", 0.0)))
        return self._targeted_mutation(comp, sig, base_rules)

    @staticmethod
    def pareto_select(candidates: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
        """Non-dominated front — lower σ, latency, and cost are better (Pareto-minimize all three)."""
        front: List[Dict[str, Any]] = []
        for c in candidates:
            dominated = False
            σc = float(c.get("σ", c.get("sigma", 1.0)))
            lc = float(c.get("latency", 0.0))
            kc = float(c.get("cost", 0.0))
            for other in candidates:
                if other is c:
                    continue
                σo = float(other.get("σ", other.get("sigma", 1.0)))
                lo = float(other.get("latency", 0.0))
                ko = float(other.get("cost", 0.0))
                if (
                    σo <= σc
                    and lo <= lc
                    and ko <= kc
                    and (σo < σc or lo < lc or ko < kc)
                ):
                    dominated = True
                    break
            if not dominated:
                front.append(c)
        return front

    def _total_cost(self) -> float:
        return float(self._run_cost_accum)

    def run(
        self,
        gate: Any,
        eval_data: List[Any],
        max_steps: int = 10,
        formal: Any = None,
        *,
        mutate_sign: float = 1.0,
        max_cost: Optional[float] = None,
        step_cost: float = 0.0,
        diminishing_delta: float = 0.001,
    ) -> List[Dict[str, Any]]:
        """RSI-style steps with optional GEPA budget stop (cost) and diminishing σ gains."""
        history: List[Dict[str, Any]] = []
        g = gate
        self.history = history
        self._run_cost_accum = 0.0
        sc = max(0.0, float(step_cost))
        for _ in range(max(1, int(max_steps))):
            self._run_cost_accum += sc
            if max_cost is not None and self._total_cost() > float(max_cost):
                break
            result = self.step(g, eval_data, formal, mutate_sign=mutate_sign)
            history.append(result)
            if result.get("accepted"):
                c = result.get("candidate")
                if c is not None:
                    g = c
            if (
                len(history) >= 3
                and any(_h.get("accepted") for _h in history)
            ):
                recent = [float(_h.get("σ_after", 0.0)) for _h in history[-3:]]
                improvement = recent[0] - recent[-1]
                if improvement < float(diminishing_delta):
                    break
        self.history = history
        return history

    def _goal_drift(
        self,
        original: Any,
        mutated: Any,
        eval_data: List[Any],
        *,
        drift_threshold: float = 0.2,
    ) -> Dict[str, Any]:
        """Lab-style goal drift index on σ-margin aggregates (SAHOO-inspired; not a harness metric)."""
        σ_orig = float(self._rsi_evaluate(original, eval_data))
        σ_mut = float(self._rsi_evaluate(mutated, eval_data))
        return self._goal_drift_from_sigmas(σ_orig, σ_mut, threshold=drift_threshold)

    def _goal_drift_from_sigmas(
        self,
        σ_orig: float,
        σ_mut: float,
        *,
        threshold: float = 0.2,
    ) -> Dict[str, Any]:
        d = abs(float(σ_orig) - float(σ_mut))
        return {"drift_score": round(d, 4), "drifted": d > float(threshold)}

    def _regression_check(self, original: Any, mutated: Any, eval_data: List[Any]) -> bool:
        """True if aggregate margin stress regresses materially (>10% worse)."""
        σ_orig = float(self._rsi_evaluate(original, eval_data))
        σ_mut = float(self._rsi_evaluate(mutated, eval_data))
        return self._regression_check_scalar(σ_orig, σ_mut)

    @staticmethod
    def _regression_check_scalar(σ_orig: float, σ_mut: float) -> bool:
        o = float(σ_orig)
        if o <= 0.0:
            return float(σ_mut) > 0.0
        return float(σ_mut) > o * 1.1

    def evolve_safe(
        self,
        gate: Any,
        eval_data: List[Any],
        max_steps: int = 10,
        risk_budget: float = 1.0,
        formal: Any = None,
        *,
        mutate_sign: float = 1.0,
        mutate_fn: Optional[Callable[[Any], Any]] = None,
        drift_threshold: float = 0.2,
    ) -> Dict[str, Any]:
        """Bounded-risk propose/eval loop (lab scaffold).

        Interprets positive increments of the σ-margin aggregate as **risk** and decreases
        as **utility**. Cumulative accepted risk is capped by ``risk_budget`` (halt when the
        next proposal cannot fit). Inspired by bounded-risk RSI discussions in the literature;
        **not** a proof of any external theorem in-tree — **NOT AGI ACHIEVED**.
        See ``docs/CLAIM_DISCIPLINE.md``."""
        cumulative_risk = 0.0
        cumulative_utility = 0.0
        history: List[Dict[str, Any]] = []

        for step in range(max(1, int(max_steps))):
            baseline_σ = float(self._rsi_evaluate(gate, eval_data))
            if mutate_fn is not None:
                candidate = mutate_fn(gate)
            else:
                candidate = self._rsi_mutate(gate, sign=mutate_sign)
            candidate_σ = float(self._rsi_evaluate(candidate, eval_data))
            δ = max(0.0, candidate_σ - baseline_σ)
            utility = max(0.0, baseline_σ - candidate_σ)

            budget = float(risk_budget)
            if cumulative_risk + δ > budget:
                history.append(
                    {
                        "step": step,
                        "action": "HALT",
                        "reason": "risk budget exhausted",
                        "cumulative_risk": round(cumulative_risk, 4),
                    }
                )
                break

            drift = self._goal_drift_from_sigmas(baseline_σ, candidate_σ, threshold=drift_threshold)

            if formal is not None and hasattr(formal, "check_invariants"):
                try:
                    if not bool(formal.check_invariants(candidate)):
                        history.append(
                            {
                                "step": step,
                                "action": "REJECT",
                                "reason": "invariant violation",
                            }
                        )
                        continue
                except Exception:  # noqa: BLE001 — formal hook is best-effort
                    history.append(
                        {
                            "step": step,
                            "action": "REJECT",
                            "reason": "invariant check error",
                        }
                    )
                    continue

            regression = self._regression_check_scalar(baseline_σ, candidate_σ)

            if utility > δ and not drift["drifted"] and not regression:
                gate = candidate
                cumulative_risk += δ
                cumulative_utility += utility
                history.append(
                    {
                        "step": step,
                        "action": "ACCEPT",
                        "σ_before": round(baseline_σ, 4),
                        "σ_after": round(candidate_σ, 4),
                        "δ": round(δ, 4),
                        "utility": round(utility, 4),
                        "cumulative_risk": round(cumulative_risk, 4),
                        "cumulative_utility": round(cumulative_utility, 4),
                    }
                )
            else:
                if drift["drifted"]:
                    reason = "drift"
                elif regression:
                    reason = "regression"
                else:
                    reason = "negative utility"
                history.append(
                    {
                        "step": step,
                        "action": "REJECT",
                        "reason": reason,
                    }
                )

        return {
            "history": history,
            "final_risk": round(cumulative_risk, 4),
            "final_utility": round(cumulative_utility, 4),
            "risk_budget_remaining": round(float(risk_budget) - cumulative_risk, 4),
            "steps_taken": len(history),
            "accepted": sum(1 for h in history if h.get("action") == "ACCEPT"),
        }
