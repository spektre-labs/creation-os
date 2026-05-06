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
                return {"accepted": False, "reason": "invariant violation"}
        if candidate_sigma < baseline:
            return {
                "accepted": True,
                "σ_before": baseline,
                "σ_after": candidate_sigma,
                "improvement": baseline - candidate_sigma,
                "candidate": candidate,
            }
        return {"accepted": False, "reason": "no improvement"}

    def run(
        self,
        gate: Any,
        eval_data: List[Any],
        max_steps: int = 10,
        formal: Any = None,
        *,
        mutate_sign: float = 1.0,
    ) -> List[Dict[str, Any]]:
        history: List[Dict[str, Any]] = []
        g = gate
        for _ in range(max(1, int(max_steps))):
            result = self.step(g, eval_data, formal, mutate_sign=mutate_sign)
            history.append(result)
            if result.get("accepted"):
                c = result.get("candidate")
                if c is not None:
                    g = c
        return history
