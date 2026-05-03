# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-continual v3 — lab scaffolding for online LoRA stacks, merge, and σ-regression hooks.

No training backend is bundled: vectors stand in for adapter weights so merge and
orthogonal init stay testable in pure Python. For evidence boundaries see
``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import time
from typing import Any, Dict, List, Optional

__all__ = ["SigmaContinual"]


class SigmaContinual:
    """Task-agnostic continual-learning **lab** object with σ-gated steps."""

    def __init__(
        self,
        gate: Any = None,
        model: Any = None,
        *,
        rank: int = 8,
        merge_alpha: float = 0.5,
    ) -> None:
        from cos.sigma_gate import SigmaGate

        self.gate = gate or SigmaGate()
        self.model = model
        self.rank = int(rank)
        self.merge_alpha = float(merge_alpha)

        self.lora_stack: List[Dict[str, List[float]]] = []
        self.merged_lora: Optional[Dict[str, List[float]]] = None
        self.loss_history: List[float] = []
        self.sigma_history: List[float] = []

        self.skill_baselines: Dict[str, float] = {}

    def learn(self, data: Any, task_id: Optional[str] = None) -> Dict[str, Any]:
        """Detect shift, create/update LoRA sketch, merge if σ allows, run regression."""
        t0 = time.monotonic()

        shift_detected = self._detect_shift(data)

        if shift_detected or not self.lora_stack:
            new_lora = self._init_orthogonal_lora()
        else:
            new_lora = self.lora_stack[-1] if self.lora_stack else self._init_lora()

        train_result = self._train_lora(new_lora, data)
        del train_result

        sigma = float(self._evaluate_lora(new_lora, data))

        if sigma > 0.7:
            return {
                "learned": False,
                "reason": "new LoRA unreliable",
                "sigma": sigma,
            }

        if self.merged_lora:
            self.merged_lora = self._merge_loras(self.merged_lora, new_lora, self.merge_alpha)
        else:
            self.merged_lora = new_lora

        self.lora_stack.append(new_lora)
        self.sigma_history.append(sigma)

        regression = self.check_regression()

        elapsed = (time.monotonic() - t0) * 1000.0

        return {
            "learned": True,
            "task_id": task_id or f"task_{len(self.lora_stack)}",
            "sigma": round(sigma, 4),
            "shift_detected": shift_detected,
            "regression": regression,
            "lora_count": len(self.lora_stack),
            "elapsed_ms": round(elapsed, 2),
        }

    def check_regression(self) -> Dict[str, Any]:
        """Compare each registered skill baseline σ to a simple replay estimate."""
        regressions: List[Dict[str, Any]] = []

        for skill, baseline_sigma in self.skill_baselines.items():
            current_sigma = float(self._evaluate_skill(skill))
            delta = current_sigma - float(baseline_sigma)

            if delta > 0.15:
                regressions.append(
                    {
                        "skill": skill,
                        "baseline_sigma": round(float(baseline_sigma), 4),
                        "current_sigma": round(current_sigma, 4),
                        "delta": round(delta, 4),
                    }
                )

        return {
            "regressed": len(regressions) > 0,
            "regressions": regressions,
            "skills_checked": len(self.skill_baselines),
            "skills_retained": len(self.skill_baselines) - len(regressions),
        }

    def register_skill(self, name: str, test_prompt: str, test_response: str) -> None:
        """Record a σ baseline for a named skill (replay probe)."""
        sigma, _ = self.gate.score(str(test_prompt), str(test_response))
        self.skill_baselines[str(name)] = float(sigma)

    def _detect_shift(self, data: Any) -> bool:
        """Treat a flat recent loss curve as a distribution-shift cue."""
        del data
        if len(self.loss_history) < 5:
            return True

        recent = self.loss_history[-5:]
        mean = sum(recent) / len(recent)
        variance = sum((x - mean) ** 2 for x in recent) / len(recent)
        return variance < 0.001

    def _init_orthogonal_lora(self) -> Dict[str, List[float]]:
        """Gram–Schmidt-style peel against ``merged_lora`` when present."""
        if not self.merged_lora:
            return self._init_lora()

        new = self._init_lora()
        for key in new:
            if key in self.merged_lora:
                old = self.merged_lora[key]
                new_vec = new[key]
                dot = sum(a * b for a, b in zip(new_vec, old))
                norm = sum(a * a for a in old) + 1e-8
                projection = dot / norm
                new[key] = [a - projection * b for a, b in zip(new_vec, old)]

        return new

    def _init_lora(self) -> Dict[str, List[float]]:
        return {"weights": [0.0] * self.rank}

    def _train_lora(self, lora: Dict[str, List[float]], data: Any) -> Dict[str, float]:
        del lora, data
        self.loss_history.append(0.1)
        return {"loss": 0.1}

    def _evaluate_lora(self, lora: Dict[str, List[float]], data: Any) -> float:
        del lora, data
        return 0.2

    def _evaluate_skill(self, skill: str) -> float:
        """Placeholder: replay with stored baseline until a backend supplies logits."""
        return float(self.skill_baselines.get(skill, 0.5))

    def _merge_loras(
        self,
        old: Dict[str, List[float]],
        new: Dict[str, List[float]],
        alpha: float,
    ) -> Dict[str, List[float]]:
        merged: Dict[str, List[float]] = {}
        keys = set(old.keys()) | set(new.keys())
        for key in keys:
            if key in old and key in new:
                merged[key] = [
                    alpha * a + (1.0 - alpha) * b for a, b in zip(old[key], new[key])
                ]
            elif key in old:
                merged[key] = list(old[key])
            else:
                merged[key] = list(new[key])
        return merged

    def stats(self) -> Dict[str, Any]:
        return {
            "lora_count": len(self.lora_stack),
            "skills_tracked": len(self.skill_baselines),
            "loss_history_len": len(self.loss_history),
            "merged": self.merged_lora is not None,
        }
