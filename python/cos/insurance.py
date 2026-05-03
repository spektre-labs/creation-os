# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-insurance — illustrative risk scoring for underwriting **lab** workflows.

Underwriters must run independent models. Nothing here is an insurance product, binder, or
actuarial filing. See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import statistics
from typing import Any, Dict, Mapping, Optional, Sequence

__all__ = ["SigmaInsurance"]


class SigmaInsurance:
    """Map σ telemetry + benchmark metadata to a 0–100 risk score (higher = riskier)."""

    def risk_score(
        self,
        gate: Any,
        bench_results: Mapping[str, Any],
        deployment_context: Mapping[str, Any],
    ) -> Dict[str, Any]:
        del gate, deployment_context
        sigma_avg = float(bench_results.get("sigma_avg", 0.45))
        auroc = bench_results.get("AUROC")
        ladder_ok = bool(bench_results.get("evidence_ladder_negatives"))
        base = 40.0 + 60.0 * min(1.0, sigma_avg / 0.9)
        if auroc is not None:
            base -= max(0.0, float(auroc) - 0.5) * 40.0
        if ladder_ok:
            base -= 8.0
        score = int(max(0, min(100, round(base))))
        return {"risk_score_0_100": score, "inputs": dict(bench_results), "note": "Toy composite; not an actuarial standard."}

    @staticmethod
    def policy_requirements(risk_score: int) -> Dict[str, Any]:
        rs = int(max(0, min(100, risk_score)))
        reqs = [
            "Maintain calibrated σ-gate with archived repro metadata.",
            "Quarterly drift review on decision distributions.",
        ]
        if rs < 35:
            reqs.append("sigma_avg_target_below_0.2")
            reqs.append("abstention_rate_monitor_above_0.05")
        else:
            reqs.append("human_review_all_rethink")
            reqs.append("monthly_incident_review")
        return {"risk_score": rs, "requirements": reqs}

    @staticmethod
    def incident_report(event: Mapping[str, Any], sigma_at_time: float, verdict_at_time: str) -> Dict[str, Any]:
        return {
            "event_id": event.get("id", "unknown"),
            "description": str(event.get("description", "")),
            "sigma_at_time": float(sigma_at_time),
            "verdict_at_time": str(verdict_at_time).upper(),
            "timestamp": event.get("timestamp"),
        }

    def actuarial_data(self, history: Sequence[Mapping[str, Any]]) -> Dict[str, Any]:
        sigmas = [float(h.get("sigma", 0.5)) for h in history]
        if not sigmas:
            return {"n": 0, "mean_sigma": 0.5}
        return {
            "n": len(sigmas),
            "mean_sigma": round(float(statistics.mean(sigmas)), 6),
            "std_sigma": round(float(statistics.pstdev(sigmas)), 6) if len(sigmas) > 1 else 0.0,
        }

    def certificate(
        self,
        gate: Any,
        bench: Mapping[str, Any],
        compliance: Optional[Mapping[str, Any]] = None,
    ) -> Dict[str, Any]:
        del gate
        rs = self.risk_score(
            None,
            bench,
            {},
        )
        return {
            "label": "Spektre Verified (lab certificate stub)",
            "risk_score": rs["risk_score_0_100"],
            "compliance_pack_present": compliance is not None,
            "disclaimer": (
                "Not EU AI Act conformity; not an insurance warranty; marketing copy must stay claim-disciplined."
            ),
        }
