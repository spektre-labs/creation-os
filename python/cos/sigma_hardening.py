# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
"""Adversarial follow-up from σ-red-team **bypass** cases (training pipeline hook).

Real retraining belongs in ``benchmarks/sigma_gate_lsd/`` (or your harness).
This class collects examples and delegates to ``gate.retrain_with_adversarial``
only when that method exists.
"""
from __future__ import annotations

from typing import Any, Dict, List


class SigmaHardening:
    """Collect ACCEPT-bypass cases for export or optional hook retraining."""

    def harden(self, gate: Any, red_team_results: Dict[str, Dict[str, Any]]) -> Any:
        bypassed: List[Dict[str, Any]] = []
        for attack_name, results in red_team_results.items():
            for case in results.get("bypassed_cases") or []:
                bypassed.append(
                    {
                        "prompt": case.get("prompt", ""),
                        "response": case.get("response", ""),
                        "label": "hallucinated",
                        "attack": attack_name,
                        "sigma": case.get("sigma"),
                        "verdict": case.get("verdict"),
                    }
                )

        if not bypassed:
            return "No bypasses — gate is already robust on this batch."

        if hasattr(gate, "retrain_with_adversarial"):
            out = gate.retrain_with_adversarial(bypassed)
            return out if out is not None else f"Retrained with {len(bypassed)} adversarial examples"

        return {
            "status": "export_only",
            "n_examples": len(bypassed),
            "message": (
                "Gate has no retrain_with_adversarial(); export JSONL for sigma_gate_lsd / trainer."
            ),
            "examples": bypassed,
        }


__all__ = ["SigmaHardening"]
