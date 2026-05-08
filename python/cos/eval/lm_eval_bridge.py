# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Optional bridge from EleutherAI ``lm-eval`` result JSON to a σ-shaped summary layer.

**Evidence class:** this does **not** replace a full harness archive; it post-processes
reported task metrics. Bind headline numbers to raw ``lm_eval`` JSON per
``docs/REPRO_BUNDLE_TEMPLATE.md``."""
from __future__ import annotations

import json
from pathlib import Path
from typing import Any, Dict, List, Optional, Union

from cos.sigma_gate import SigmaGate

__all__ = ["LMEvalBridge"]


def _task_accuracy(metrics: Dict[str, Any]) -> Optional[float]:
    """Best-effort extract of a scalar accuracy from lm-eval task metrics."""
    if not isinstance(metrics, dict):
        return None
    for key in ("acc,none", "acc_norm,none", "acc", "exact_match,none"):
        v = metrics.get(key)
        if isinstance(v, (int, float)):
            return float(v)
    # sometimes nested
    for _k, v in metrics.items():
        if isinstance(v, dict):
            inner = _task_accuracy(v)
            if inner is not None:
                return inner
    return None


def _sigma_from_acc(acc: Optional[float]) -> float:
    if acc is None:
        return 0.5
    a = max(0.0, min(1.0, float(acc)))
    return 1.0 - a


def _verdict_from_sigma(s: float) -> str:
    if s < 0.15:
        return "ACCEPT"
    if s < 0.5:
        return "RETHINK"
    return "ABSTAIN"


class LMEvalBridge:
    """Summarize lm-eval task rows with a scalar ``σ = 1 - acc`` style proxy (lab hook)."""

    def __init__(self, gate: Optional[Any] = None) -> None:
        self.gate = gate if gate is not None else SigmaGate()

    def score_results(self, lm_eval_json_path: Union[str, Path]) -> Dict[str, Any]:
        """Load one lm-eval ``results`` JSON; attach per-task σ summary.

        Accepts either a top-level dict with ``results``, or paths to standard harness dumps."""
        path = Path(lm_eval_json_path)
        if not path.is_file():
            return {"error": "file not found", "path": str(path)}

        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as e:
            return {"error": str(e), "path": str(path)}

        results = data.get("results")
        if not isinstance(results, dict):
            return {"error": "missing or invalid 'results' object", "path": str(path)}

        σ_results: Dict[str, Dict[str, Any]] = {}
        for task, metrics in results.items():
            acc = _task_accuracy(metrics) if isinstance(metrics, dict) else None
            σ = _sigma_from_acc(acc)
            σ_results[str(task)] = {
                "original_acc": acc,
                "σ": round(σ, 4),
                "verdict": _verdict_from_sigma(σ),
            }

        avg_σ = (
            sum(r["σ"] for r in σ_results.values()) / max(len(σ_results), 1) if σ_results else 0.0
        )
        return {
            "model": data.get("model_args", data.get("model", data.get("model_name", "unknown"))),
            "tasks": σ_results,
            "avg_σ": round(float(avg_σ), 4),
        }

    def generate_eval_config(self, tasks: Optional[List[str]] = None) -> Dict[str, Any]:
        """Example ``lm_eval`` invocation template (operator fills model / base_url)."""
        task_list = tasks or [
            "truthfulqa_mc2",
            "hellaswag",
            "arc_challenge",
            "mmlu",
            "winogrande",
        ]
        joined = ",".join(task_list)
        return {
            "command": (
                "lm_eval --model local-completions "
                '--model_args model=dummy,base_url=http://127.0.0.1:8001/v1 '
                f"--tasks {joined} "
                "--batch_size 4 "
                "--output_path eval_results/ "
                "--log_samples"
            ),
            "post_process": "python -m cos.cli repro --name lm_eval_run (after copying JSON path)",
            "tasks": list(task_list),
        }
