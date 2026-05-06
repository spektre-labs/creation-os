# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-distill — selective distillation lab using the gate as propose-and-verify mask.

Teacher/student callables are optional; the gate decides where full KD would run.
Does not implement backward passes or claim wall-clock savings; see
``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

from typing import Any, Dict, List, Sequence

__all__ = ["SigmaDistill"]


class SigmaDistill:
    """σ-masked batch steps, token difficulty from logits distance, σ curriculum sort."""

    def __init__(self) -> None:
        self.steps_applied = 0

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
