# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
cos-evolve — σ-guided recursive self-improvement (lab harness).

The system may propose changes to **evolvable** modules. Proposals pass a σ-gate,
then **anchor σ** must **decrease on average** after the (simulated) patch
(∆σ = mean(after) − mean(before); accept iff ∆σ < 0).

**Invariant:** ``sigma_gate.h``, ``sigma_gate.c``, and ``atlantean_codex.yaml`` are
never modified by this harness. The C/Python gate kernel stays canonical (see repo policy).

Ref: Ω-loop phases REFLECT + CONSOLIDATE; ICLR-style improvement-operator cards.
"""
from __future__ import annotations

import os
import re
import time
from typing import Any, Dict, List, Optional, Protocol, Tuple, runtime_checkable

from .sigma_gate_core import Verdict


@runtime_checkable
class EvolveGate(Protocol):
    def score(self, prompt: str, text: str) -> Tuple[float, Verdict]:
        ...


@runtime_checkable
class EvolveModel(Protocol):
    def generate(self, prompt: str) -> str:
        ...


@runtime_checkable
class EvolveEvaluator(Protocol):
    """Measures σ-like stress on anchor tests before/after a proposal is (simulated) applied."""

    def run_anchors(
        self,
        anchor_tests: Optional[List[str]],
        phase: str,
        card: Dict[str, Any],
    ) -> List[float]:
        ...


class ToyEvolveEvaluator:
    """Deterministic mean shift: every anchor has the same σ before; after, adds ``delta_mean``."""

    def __init__(self, mean_after_minus_before: float) -> None:
        self._delta = float(mean_after_minus_before)

    def run_anchors(
        self,
        anchor_tests: Optional[List[str]],
        phase: str,
        card: Dict[str, Any],
    ) -> List[float]:
        _ = card
        n = max(1, len(anchor_tests) if anchor_tests else 1)
        before_v = 0.35
        if phase == "before":
            return [before_v + 0.01 * i for i in range(n)]
        before_mean = sum(before_v + 0.01 * i for i in range(n)) / n
        after_mean = before_mean + self._delta
        return [after_mean + 0.01 * (i - n / 2.0) / max(n, 1) for i in range(n)]


def _normalize_target(target_module: str) -> str:
    return target_module.replace("\\", "/").strip()


def _is_immutable_target(target_module: str) -> bool:
    """Reject evolution targets that include kernel/codex invariants (basename or path)."""
    t = _normalize_target(target_module)
    immutable = (
        "sigma_gate.h",
        "sigma_gate.c",
        "atlantean_codex.yaml",
    )
    base = os.path.basename(t)
    if base in immutable:
        return True
    for im in immutable:
        if im in t:
            return True
    return False


def analyze_diff(proposal: str) -> List[str]:
    """List touched paths from a unified diff-like blob (best-effort)."""
    paths: List[str] = []
    for line in str(proposal).splitlines():
        m = re.match(r"^\+\+\+\s+(?:b/)?(.+)", line)
        if m:
            p = m.group(1).strip()
            if p not in ("/dev/null",):
                paths.append(p)
    return paths


class SigmaEvolve:
    IMMUTABLE = frozenset({"sigma_gate.h", "sigma_gate.c", "atlantean_codex.yaml"})

    def __init__(
        self,
        gate: EvolveGate,
        model: EvolveModel,
        evaluator: EvolveEvaluator,
        *,
        archive: Optional["SigmaEvolutionArchive"] = None,
    ) -> None:
        self.gate = gate
        self.model = model
        self.evaluator = evaluator
        self.archive = archive
        self.generations: List[Dict[str, Any]] = []
        self.current_gen = int(0)

    def assess_risks(self, proposal: str, target: str) -> List[str]:
        risks: List[str] = []
        blob = f"{proposal}\n{target}"
        low = blob.lower()
        if "sigma_gate.h" in low or "sigma_gate.c" in low:
            risks.append("CRITICAL: proposal references immutable gate sources")
        if "import os" in blob or "subprocess" in blob:
            risks.append("HIGH: proposal mentions OS/subprocess interfaces")
        if "eval(" in blob or "exec(" in blob:
            risks.append("CRITICAL: proposal mentions eval/exec")
        lines = len(str(proposal).splitlines())
        if lines > 100:
            risks.append(f"MEDIUM: large diff ({lines} lines)")
        return risks

    def propose_improvement(self, target_module: str, goal: str) -> Dict[str, Any]:
        if _is_immutable_target(target_module):
            return {
                "rejected": True,
                "reason": f"immutable_target:{target_module!r}",
            }

        prompt = (
            f"Propose an improvement to {_normalize_target(target_module)} to achieve: {goal}. "
            "Output a unified diff patch only."
        )
        proposal = self.model.generate(prompt)
        sigma, verdict = self.gate.score(goal, proposal)
        modifies = analyze_diff(proposal)
        card: Dict[str, Any] = {
            "generation_planned": self.current_gen + 1,
            "target": _normalize_target(target_module),
            "goal": goal,
            "proposal": proposal,
            "proposal_sigma": float(sigma),
            "proposal_verdict": verdict.name,
            "modifies": modifies,
            "risks": self.assess_risks(proposal, target_module),
            "rollback_plan": f"git checkout HEAD -- {_normalize_target(target_module)}",
        }
        if verdict != Verdict.ACCEPT:
            card["status"] = "rejected"
            card["reason"] = f"σ-gate verdict {verdict.name} (σ={sigma:.4f})"
            return card
        return card

    def compute_delta_sigma(self, before_sigmas: List[float], after_sigmas: List[float]) -> float:
        if not before_sigmas or not after_sigmas:
            return 0.0
        b = sum(before_sigmas) / len(before_sigmas)
        a = sum(after_sigmas) / len(after_sigmas)
        return float(a - b)

    def save_state(self) -> Dict[str, Any]:
        return {"current_gen": self.current_gen, "n_accepted": len(self.generations)}

    def restore_state(self, checkpoint: Dict[str, Any]) -> None:
        self.current_gen = int(checkpoint.get("current_gen", 0))

    def apply_patch(self, proposal: str) -> None:
        """Lab no-op: real deployments would apply ``proposal`` inside a sandbox."""
        _ = proposal

    def run_eval(self, anchor_tests: Optional[List[str]], label: str, card: Dict[str, Any]) -> List[float]:
        return list(self.evaluator.run_anchors(anchor_tests, label, card))

    def apply_and_test(
        self,
        card: Dict[str, Any],
        anchor_tests: Optional[List[str]],
    ) -> Dict[str, Any]:
        if card.get("rejected") or card.get("status") == "rejected":
            return {"accepted": False, "reason": "invalid_or_rejected_card", "delta_sigma": 0.0}
        if "proposal" not in card:
            return {"accepted": False, "reason": "missing proposal", "delta_sigma": 0.0}

        checkpoint = self.save_state()
        try:
            before_sigmas = self.run_eval(anchor_tests, "before", card)
            self.apply_patch(str(card["proposal"]))
            after_sigmas = self.run_eval(anchor_tests, "after", card)
            delta = self.compute_delta_sigma(before_sigmas, after_sigmas)

            if delta < 0:
                self.current_gen += 1
                rec = {
                    **{k: v for k, v in card.items() if k != "proposal"},
                    "status": "accepted",
                    "delta_sigma": delta,
                    "generation": self.current_gen,
                    "before_sigmas": before_sigmas,
                    "after_sigmas": after_sigmas,
                }
                self.generations.append(rec)
                after_mean = sum(after_sigmas) / len(after_sigmas)
                if self.archive is not None:
                    snap = str(card.get("proposal", ""))[:4000]
                    self.archive.add(self.current_gen, after_mean, snap)
                return {"accepted": True, "delta_sigma": delta, "gen": self.current_gen}
            self.restore_state(checkpoint)
            return {"accepted": False, "delta_sigma": float(delta), "reason": "σ did not decrease (∆σ ≥ 0)"}
        except Exception as e:
            self.restore_state(checkpoint)
            return {"accepted": False, "delta_sigma": 0.0, "reason": f"eval_error:{e}"}

    def evolve_loop(
        self,
        target_module: str,
        goal: str,
        max_generations: int = 10,
        anchor_tests: Optional[List[str]] = None,
    ) -> Dict[str, Any]:
        results: List[Dict[str, Any]] = []
        for _ in range(max(1, int(max_generations))):
            card = self.propose_improvement(target_module, goal)
            if card.get("rejected") or card.get("status") == "rejected":
                results.append(card)
                continue
            res = self.apply_and_test(card, anchor_tests)
            results.append(res)
        accepted_n = sum(1 for r in results if r.get("accepted") is True)
        return {
            "generations": len(results),
            "accepted": accepted_n,
            "total_delta_sigma": float(sum(float(r.get("delta_sigma", 0) or 0) for r in results)),
            "results": results,
        }


class SigmaEvolutionArchive:
    """Pareto / best tracking over generations (Gödel-machine style archive)."""

    def __init__(self) -> None:
        self.archive: List[Dict[str, Any]] = []

    def add(self, generation: int, fitness_sigma: float, code_snapshot: str) -> None:
        self.archive.append(
            {
                "generation": int(generation),
                "fitness": float(1.0 - float(fitness_sigma)),
                "sigma": float(fitness_sigma),
                "code": str(code_snapshot),
                "timestamp": time.time(),
            }
        )

    def best(self) -> Optional[Dict[str, Any]]:
        if not self.archive:
            return None
        return min(self.archive, key=lambda x: float(x["sigma"]))

    def pareto_front(self) -> List[Dict[str, Any]]:
        front: List[Dict[str, Any]] = []
        for a in self.archive:
            dominated = False
            for b in self.archive:
                if float(b["sigma"]) < float(a["sigma"]) and len(str(b["code"])) <= len(str(a["code"])):
                    dominated = True
                    break
            if not dominated:
                front.append(a)
        return front

    def rollback_to_generation(self, gen: int) -> None:
        """Drop archive entries strictly after ``gen`` (lab)."""
        g = int(gen)
        self.archive = [x for x in self.archive if int(x.get("generation", 0)) <= g]


__all__ = [
    "EvolveEvaluator",
    "EvolveGate",
    "EvolveModel",
    "SigmaEvolutionArchive",
    "SigmaEvolve",
    "ToyEvolveEvaluator",
    "analyze_diff",
]
