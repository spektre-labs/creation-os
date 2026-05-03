# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
σ-TTT **v150** (v2 loop): gate-guided test-time adaptation + time-budgeted best-of-N.

This module is **torch-free** so `pytest` can run on hosts without PyTorch; v123 layers remain in
:mod:`cos.sigma_ttt`. Does not modify ``sigma_gate.h``.
"""
from __future__ import annotations

import json
import time
from pathlib import Path
from typing import Any, Dict

from cos.sigma_moe import _gate_score


class TTTLabStubModel:
    """
    Reference **model** API for :class:`SigmaTTTv2`: ``generate``, checkpoint/restore,
    and a minimal self-supervised ``ttt_step`` hook (mask → loss → fast-weight update).
    CLI uses this when no external backend is configured.
    """

    def __init__(self) -> None:
        self._adapt: int = 0

    def save_state(self) -> Any:
        return {"adapt": int(self._adapt)}

    def load_state(self, checkpoint: Any) -> None:
        if isinstance(checkpoint, dict) and "adapt" in checkpoint:
            self._adapt = int(checkpoint["adapt"])

    def generate(self, prompt: str, *, temperature: float = 0.0) -> str:
        pl = prompt.lower()
        if "2+2" in pl.replace(" ", "") or "what is 2+2" in pl:
            return "4"
        if float(temperature) >= 0.7:
            return "TTT_REFINED: proof sketch via test-time adaptation."
        if self._adapt == 0:
            return "TTT_STEP0_UNCERTAIN"
        if self._adapt < 3:
            return f"TTT_REFINING_{self._adapt}"
        return "TTT_REFINED: proof sketch via test-time adaptation."

    def mask_random_tokens(self, text: str, mask_ratio: float = 0.15) -> str:
        del mask_ratio
        return text

    def compute_loss(self, masked: str, target: str) -> float:
        del masked, target
        return 1.0 / (1.0 + float(self._adapt))

    def update_fast_weights(self, loss: Any, *, lr: float = 1e-4) -> None:
        del loss, lr
        self._adapt += 1


class TTTEarlyStopStubModel:
    """TTT steps never improve σ — used to exercise early-stop in :meth:`SigmaTTTv2.inference`."""

    def __init__(self) -> None:
        self._adapt = 0

    def save_state(self) -> Any:
        return {"adapt": int(self._adapt)}

    def load_state(self, checkpoint: Any) -> None:
        if isinstance(checkpoint, dict) and "adapt" in checkpoint:
            self._adapt = int(checkpoint["adapt"])

    def generate(self, prompt: str, *, temperature: float = 0.0) -> str:
        del prompt, temperature
        return "STUCK_HIGH_SIGMA_RESPONSE"

    def mask_random_tokens(self, text: str, mask_ratio: float = 0.15) -> str:
        del mask_ratio
        return text

    def compute_loss(self, masked: str, target: str) -> float:
        del masked, target
        return 0.25

    def update_fast_weights(self, loss: Any, *, lr: float = 1e-4) -> None:
        del loss, lr
        self._adapt += 1


class SigmaTTTv2:
    """
    σ-guided test-time training loop: cheap path skips adaptation; hard prompts run temporary
    TTT steps until σ drops or early-stop. **Always** restores model state from checkpoint afterward.
    """

    def __init__(self, model: Any, gate: Any, *, max_steps: int = 10) -> None:
        self.model = model
        self.gate = gate
        self.max_steps = max(1, int(max_steps))
        self.stats: Dict[str, Any] = {"ttt_skipped": 0, "ttt_applied": 0, "avg_steps": 0.0}

    def ttt_step(self, prompt: str, previous_response: str) -> None:
        del previous_response
        masked = self.model.mask_random_tokens(prompt, mask_ratio=0.15)
        loss = self.model.compute_loss(masked, prompt)
        self.model.update_fast_weights(loss, lr=1e-4)

    def inference(self, *, prompt: str, force_ttt: bool = False) -> Dict[str, Any]:
        response = str(self.model.generate(prompt))
        sigma, verdict = _gate_score(self.gate, prompt, response)

        if verdict == "ACCEPT" and float(sigma) < 0.15 and not force_ttt:
            self.stats["ttt_skipped"] = int(self.stats["ttt_skipped"]) + 1
            return {
                "response": response,
                "sigma": float(sigma),
                "verdict": verdict,
                "ttt_steps": 0,
                "reason": "low σ, TTT not needed",
            }

        checkpoint = self.model.save_state()
        best_response = response
        best_sigma = float(sigma)
        best_verdict = verdict
        steps_used = 0

        for step in range(self.max_steps):
            self.ttt_step(prompt, response)
            response = str(self.model.generate(prompt))
            sigma, verdict = _gate_score(self.gate, prompt, response)
            sf, vf = float(sigma), str(verdict)
            steps_used = step + 1
            if sf < best_sigma:
                best_response = response
                best_sigma = sf
                best_verdict = vf
            if sf < 0.1:
                break
            if step > 2 and sf >= best_sigma - 1e-9:
                break

        self.model.load_state(checkpoint)

        self.stats["ttt_applied"] = int(self.stats["ttt_applied"]) + 1
        n = int(self.stats["ttt_applied"])
        prev_avg = float(self.stats["avg_steps"])
        self.stats["avg_steps"] = (prev_avg * (n - 1) + float(steps_used)) / max(n, 1)

        _, final_v = _gate_score(self.gate, prompt, best_response)
        return {
            "response": best_response,
            "sigma": best_sigma,
            "verdict": final_v,
            "verdict_best_step": best_verdict,
            "ttt_steps": int(steps_used),
        }

    def adaptive_compute(self, *, prompt: str, budget_ms: float = 1000.0) -> Dict[str, Any]:
        t0 = time.monotonic()
        response = str(self.model.generate(prompt))
        sigma, verdict = _gate_score(self.gate, prompt, response)
        best: Dict[str, Any] = {"response": response, "sigma": float(sigma), "verdict": verdict, "steps": 0}
        step = 0
        while True:
            elapsed_ms = (time.monotonic() - t0) * 1000.0
            remaining = float(budget_ms) - elapsed_ms
            if remaining < 50.0:
                break
            if float(sigma) < 0.1:
                break
            temp = 0.7 + min(step, 5) * 0.1
            response = str(self.model.generate(prompt, temperature=temp))
            sigma, verdict = _gate_score(self.gate, prompt, response)
            step += 1
            if float(sigma) < float(best["sigma"]):
                best = {
                    "response": response,
                    "sigma": float(sigma),
                    "verdict": verdict,
                    "steps": int(step),
                }

        elapsed_ms = (time.monotonic() - t0) * 1000.0
        return {**best, "elapsed_ms": float(elapsed_ms), "budget_ms": float(budget_ms)}


def default_sigma_ttt_v2_stats_path() -> Path:
    d = Path.home() / ".cos"
    d.mkdir(parents=True, exist_ok=True)
    return d / "sigma_ttt_v2_stats.json"


def load_sigma_ttt_v2_stats(path: Path, engine: SigmaTTTv2) -> None:
    if not path.is_file():
        return
    try:
        blob = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return
    stats = blob.get("stats")
    if isinstance(stats, dict):
        for k in ("ttt_skipped", "ttt_applied"):
            if k in stats:
                try:
                    engine.stats[k] = int(stats[k])
                except (TypeError, ValueError):
                    pass
        if "avg_steps" in stats:
            try:
                engine.stats["avg_steps"] = float(stats["avg_steps"])
            except (TypeError, ValueError):
                pass


def save_sigma_ttt_v2_stats(path: Path, engine: SigmaTTTv2) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps({"stats": dict(engine.stats)}, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )


__all__ = [
    "SigmaTTTv2",
    "TTTLabStubModel",
    "TTTEarlyStopStubModel",
    "default_sigma_ttt_v2_stats_path",
    "load_sigma_ttt_v2_stats",
    "save_sigma_ttt_v2_stats",
]
