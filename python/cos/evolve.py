# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-evolve v2 — lab loop for propose / evaluate / σ-gate / rollback.

This is **not** recursive self-improvement in the strong sense and **not** an AGI claim.
Each accepted change is gated by σ movement and optional safety caps. See
``docs/CLAIM_DISCIPLINE.md`` and README evidence ladder: **NOT AGI ACHIEVED**."""
from __future__ import annotations

import copy
import hashlib
import json
import time
from typing import Any, Callable, Dict, List, Optional

__all__ = ["SigmaEvolve"]

MutationTarget = str


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
        """Lower is better: mean σ plus light penalty on verdict vs optional boolean label."""
        if not eval_data:
            return 0.0
        total = 0.0
        for row in eval_data:
            if len(row) == 3:
                p, r, want_ok = str(row[0]), str(row[1]), bool(row[2])
                sigma, verdict = gate.score(p, r)
                cost = float(sigma)
                if want_ok and verdict in ("ABSTAIN", "RETHINK"):
                    cost += 0.2
                if (not want_ok) and verdict == "ACCEPT":
                    cost += 0.3
                total += cost
            else:
                p, r = str(row[0]), str(row[1])
                sigma, _ = gate.score(p, r)
                total += float(sigma)
        return total / len(eval_data)

    def _rsi_mutate(self, gate: Any) -> Any:
        """Threshold nudge on a copied σ-gate (lite lab; LSD shares inner via shallow copy)."""
        g = copy.copy(gate)
        ta = min(0.55, float(getattr(g, "threshold_accept", 0.15)) + 0.12)
        g.threshold_accept = ta
        return g

    def step(self, gate: Any, eval_data: List[Any], formal: Optional[Any] = None) -> Dict[str, Any]:
        """One RSI-style step: evaluate → mutate → optional formal check → accept if cost drops."""
        baseline_σ = self._rsi_evaluate(gate, eval_data)
        candidate = self._rsi_mutate(gate)
        if formal is not None:
            invariant_ok = bool(formal.check_invariants(candidate))
            if not invariant_ok:
                return {"accepted": False, "reason": "invariant violation", "candidate": candidate}
        candidate_σ = self._rsi_evaluate(candidate, eval_data)
        if candidate_σ < baseline_σ:
            return {
                "accepted": True,
                "σ_before": baseline_σ,
                "σ_after": candidate_σ,
                "improvement": baseline_σ - candidate_σ,
                "candidate": candidate,
            }
        return {"accepted": False, "reason": "no improvement", "candidate": candidate}

    def run(self, gate: Any, eval_data: List[Any], max_steps: int = 10, formal: Optional[Any] = None) -> List[Dict[str, Any]]:
        """Multi-step RSI loop; promotes ``gate`` when an accepted candidate appears."""
        history: List[Dict[str, Any]] = []
        cur = gate
        for _ in range(max(1, int(max_steps))):
            result = self.step(cur, eval_data, formal)
            history.append(result)
            if result.get("accepted"):
                cur = result["candidate"]
        return history
