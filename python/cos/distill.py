# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-validated knowledge distillation lab (no backward pass in-tree).

**P–KD–Q pipeline (ordering hint):** prune → distill → quantize for compression with
capability preservation — σ from :class:`~cos.sigma_gate.SigmaGate` scores **teacher** vs
**student** outputs on the same prompt; ``gap = σ_student − σ_teacher`` flags regression.

Also exposes σ-masked batch steps, token difficulty, curriculum sort (legacy helpers).
See ``docs/CLAIM_DISCIPLINE.md`` — this module does not claim wall-clock or harness accuracy."""
from __future__ import annotations

from typing import Any, Dict, List, Optional, Sequence, Tuple

from cos.sigma_gate import SigmaGate

__all__ = ["SigmaDistill"]

KdExample = Tuple[str, str, str]


class SigmaDistill:
    """σ-gated distillation validation + selective KD lab utilities."""

    def __init__(self, gate: Optional[Any] = None, tolerance: float = 0.1) -> None:
        self.gate = gate if gate is not None else SigmaGate()
        self.tolerance = float(tolerance)
        self.results: List[Dict[str, Any]] = []
        self.steps_applied = 0

    def evaluate_pair(self, prompt: str, teacher_response: str, student_response: str) -> Dict[str, Any]:
        """Compare teacher vs student σ on one example; ``gap = σ_student − σ_teacher``."""
        σ_teacher, _v_teacher = self.gate.score(str(prompt), str(teacher_response))
        σ_student, _v_student = self.gate.score(str(prompt), str(student_response))
        gap = float(σ_student) - float(σ_teacher)
        ok = gap <= self.tolerance
        return {
            "prompt": str(prompt)[:100],
            "σ_teacher": round(float(σ_teacher), 4),
            "σ_student": round(float(σ_student), 4),
            "gap": round(gap, 4),
            "acceptable": ok,
            "verdict": "PASS" if ok else "FAIL",
        }

    def validate(self, test_data: Sequence[KdExample]) -> Dict[str, Any]:
        """Score a list of ``(prompt, teacher_response, student_response)`` tuples."""
        self.results = []
        for prompt, teacher, student in test_data:
            self.results.append(self.evaluate_pair(prompt, teacher, student))

        n = len(self.results)
        if n == 0:
            return {
                "total": 0,
                "passed": 0,
                "failed": 0,
                "pass_rate": 0.0,
                "avg_gap": 0.0,
                "max_gap": 0.0,
                "verdict": "REJECT",
                "worst_cases": [],
            }

        passed = sum(1 for r in self.results if r["acceptable"])
        gaps = [float(r["gap"]) for r in self.results]
        if passed == n:
            v_all = "ACCEPT"
        elif passed > n * 0.8:
            v_all = "RETHINK"
        else:
            v_all = "REJECT"

        return {
            "total": n,
            "passed": passed,
            "failed": n - passed,
            "pass_rate": round(passed / n, 4),
            "avg_gap": round(sum(gaps) / n, 4),
            "max_gap": round(max(gaps), 4),
            "verdict": v_all,
            "worst_cases": sorted(self.results, key=lambda r: float(r["gap"]), reverse=True)[:5],
        }

    def recommend_compression(self, sigma_baseline: float, target_sigma_budget: float) -> Dict[str, Any]:
        """Heuristic compression tier from allowable σ headroom (P–KD–Q ordering in prose only)."""
        budget = float(target_sigma_budget) - float(sigma_baseline)
        if budget <= 0:
            return {"recommendation": "no compression — already at budget", "max_compression": "none"}
        if budget < 0.05:
            return {
                "recommendation": "quantization only (Q6_K or Q5_K_M)",
                "expected_σ_increase": 0.02,
            }
        if budget < 0.15:
            return {
                "recommendation": "distillation to ~50% size + Q4_K_M",
                "expected_σ_increase": 0.10,
            }
        return {
            "recommendation": "aggressive: prune 70% + distill + Q3_K_M",
            "expected_σ_increase": 0.20,
        }

    def distill_step(
        self,
        teacher: Any,
        student: Any,
        batch: List[Dict[str, str]],
        gate: Any,
    ) -> Dict[str, Any]:
        del teacher, student
        self.steps_applied += 1
        rows: List[Dict[str, Any]] = []
        full_kd_flags: List[bool] = []
        for item in batch:
            prompt = str(item.get("prompt", ""))
            stu = str(item.get("student_text", ""))
            sigma = float(gate.compute_sigma(None, None, prompt, stu))
            verdict = str(gate._verdict(sigma))
            need_full = sigma > float(gate.threshold_accept) or verdict != "ACCEPT"
            full_kd_flags.append(bool(need_full))
            rows.append(
                {
                    "sigma": round(sigma, 6),
                    "verdict": verdict,
                    "full_kd": bool(need_full),
                }
            )
        kd_frac = sum(1 for f in full_kd_flags if f) / max(len(full_kd_flags), 1)
        return {"rows": rows, "kd_fraction": round(kd_frac, 6), "batch_size": len(batch)}

    def selective_tokens(
        self,
        teacher_logits: Sequence[float],
        student_logits: Sequence[float],
        gate: Any,
    ) -> Dict[str, Any]:
        """Mark tokens where |Δlogit| is large **and** gate sees noisy span as high-σ."""
        idx: List[int] = []
        tlist = [float(x) for x in teacher_logits]
        slist = [float(x) for x in student_logits]
        for i, (t, s) in enumerate(zip(tlist, slist)):
            piece = f"tok{i}:{t:.4f}|{s:.4f}"
            sigma = float(gate.compute_sigma(None, None, "kd_token", piece))
            if abs(t - s) > 0.5 or sigma > float(gate.threshold_accept):
                idx.append(i)
        return {"need_help": idx, "count": len(idx)}

    def sigma_curriculum(self, batch_sigmas: Sequence[float]) -> List[int]:
        """Return indices sorting batches from easy (low σ) to hard (high σ)."""
        pairs = sorted(enumerate(float(s) for s in batch_sigmas), key=lambda x: x[1])
        return [i for i, _ in pairs]

    def efficiency(self, distill_result: Dict[str, Any]) -> Dict[str, float]:
        """Skip fraction where student text already looks acceptable to the gate."""
        kd = float(distill_result.get("kd_fraction", 0.0))
        return {"skip_fraction": round(1.0 - kd, 6), "kd_fraction": kd}
