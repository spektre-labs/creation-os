# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
σ-filtered **distillation** harness: only teacher trajectories that pass the cognitive
interrupt (``Verdict.ACCEPT`` after a stabilizing ``sigma_update`` pair) become student
training rows; optional σ-weighting and a residual "hard case" band for RAG-style relabel.

**Lab scaffold:** ``teacher``, ``student``, and ``gate`` are injectable objects — wire a
real HF stack or JSONL-only offline paths in research code. See ``docs/CLAIM_DISCIPLINE.md``.
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Callable, Dict, Iterable, List, Optional, Tuple

from .sigma_gate_core import SigmaState, Verdict, sigma_gate, sigma_update


def _stabilized_verdict(sigma_01: float, k_raw: float) -> Tuple[Verdict, SigmaState]:
    """
    Two identical float updates so ``d_sigma == 0`` at read time (matches stable-σ harness
    pattern; a single step from zero is always ``RETHINK`` in the Q16 mirror).
    """
    st = SigmaState()
    s = max(0.0, min(1.0, float(sigma_01)))
    kr = max(0.0, min(1.0, float(k_raw)))
    sigma_update(st, s, kr)
    sigma_update(st, s, kr)
    return sigma_gate(st), st


@dataclass
class SigmaDistill:
    """Filter teacher outputs through ``sigma_gate_core`` before student updates."""

    teacher: Any
    student: Any
    gate: Any
    k_raw: float = 0.95
    rethink_sigma_low: float = 0.3
    rethink_sigma_high: float = 0.7
    residual_weight: float = 2.0
    get_ground_truth: Optional[Callable[[str], Optional[str]]] = None

    def __post_init__(self) -> None:
        if self.get_ground_truth is None:
            self.get_ground_truth = lambda _p: None

    def generate_training_data(
        self,
        prompts: Iterable[str],
        *,
        limit: int = 10_000,
    ) -> List[Dict[str, Any]]:
        """
        Run ``teacher.generate``; keep rows only when the interrupt returns ``ACCEPT``
        after σ stabilization.
        """
        clean: List[Dict[str, Any]] = []
        lim = max(0, int(limit))
        for prompt in prompts:
            if len(clean) >= lim:
                break
            p = (prompt or "").strip()
            if not p:
                continue
            output = str(self.teacher.generate(p)).strip()
            if not output:
                continue
            sigma = float(self.gate.compute_sigma(self.teacher, p, output))
            sigma = max(0.0, min(1.0, sigma))
            verdict, _ = _stabilized_verdict(sigma, self.k_raw)
            if verdict != Verdict.ACCEPT:
                continue
            clean.append({"prompt": p, "response": output, "sigma": sigma})
        return clean

    def distill(self, clean_data: Iterable[Dict[str, Any]], *, epochs: int = 3) -> Any:
        """Train the student on σ-filtered rows (uniform weight)."""
        e = max(1, int(epochs))
        for _ in range(e):
            for item in clean_data:
                p = str(item.get("prompt", "")).strip()
                r = str(item.get("response", "")).strip()
                if not p or not r:
                    continue
                self.student.train_step(p, r, weight=1.0)
        return self.student

    def distill_weighted(self, clean_data: Iterable[Dict[str, Any]], *, epochs: int = 3) -> Any:
        """Same as :meth:`distill` but weight ``1 - sigma`` (low σ → higher weight)."""
        e = max(1, int(epochs))
        for _ in range(e):
            for item in clean_data:
                p = str(item.get("prompt", "")).strip()
                r = str(item.get("response", "")).strip()
                if not p or not r:
                    continue
                sig = float(item.get("sigma", 0.5))
                sig = max(0.0, min(1.0, sig))
                w = max(0.0, min(1.0, 1.0 - sig))
                self.student.train_step(p, r, weight=w)
        return self.student

    def distill_residual(self, prompts: Iterable[str]) -> Any:
        """
        Collect prompts whose teacher σ lies in ``(rethink_sigma_low, rethink_sigma_high)``
        and, when :attr:`get_ground_truth` returns text, train the student on that label.
        """
        lo = float(self.rethink_sigma_low)
        hi = float(self.rethink_sigma_high)
        if not (0.0 <= lo < hi <= 1.0):
            raise ValueError("Invalid rethink sigma band.")

        hard: List[Dict[str, Any]] = []
        for prompt in prompts:
            p = (prompt or "").strip()
            if not p:
                continue
            output = str(self.teacher.generate(p)).strip()
            if not output:
                continue
            sigma = max(0.0, min(1.0, float(self.gate.compute_sigma(self.teacher, p, output))))
            if not (lo < sigma < hi):
                continue
            hard.append({"prompt": p, "teacher_output": output, "sigma": sigma})

        for item in hard:
            correct = self.get_ground_truth(str(item["prompt"]))
            if not correct or not str(correct).strip():
                continue
            self.student.train_step(
                str(item["prompt"]).strip(),
                str(correct).strip(),
                weight=float(self.residual_weight),
            )
        return self.student


__all__ = ["SigmaDistill"]
