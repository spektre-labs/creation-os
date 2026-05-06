# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-compliance — EU AI Act **documentation packager** for operators (lab; not legal advice).

Article mappings describe how σ-gate outputs can support common transparency / oversight
workflows. They do **not** constitute statutory compliance, conformity assessment, or counsel.
See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import json
import statistics
from datetime import date, datetime, timezone
from typing import Any, Dict, List, Mapping, Optional, Sequence

__all__ = ["SigmaCompliance"]

_ART50_DEADLINE = date(2026, 8, 2)


class SigmaCompliance:
    """Assemble article-shaped JSON for internal governance; pair with counsel for filings."""

    def __init__(self, gate: Any = None) -> None:
        from cos.sigma_gate import SigmaGate

        self.gate = gate or SigmaGate()

    @staticmethod
    def deadline_check(*, as_of: Optional[date] = None) -> Dict[str, Any]:
        today = as_of or date.today()
        delta = (_ART50_DEADLINE - today).days
        return {
            "article50_applicable_date": _ART50_DEADLINE.isoformat(),
            "days_remaining": int(delta),
            "note": "Calendar stub for operator planning; verify against official journal text.",
        }

    def art9_risk_management(self, gate: Any, deployment: Mapping[str, Any]) -> Dict[str, Any]:
        """Distribution + abstention proxy from sample scores (substitute real telemetry)."""
        g = gate or self.gate
        samples = deployment.get("sample_pairs") or []
        sigmas: List[float] = []
        abst = 0
        for row in samples:
            s, v = g.score(str(row.get("prompt", "")), str(row.get("response", "")))
            sigmas.append(float(s))
            if str(v).upper() == "ABSTAIN":
                abst += 1
        n = max(len(sigmas), 1)
        if len(sigmas) >= 2:
            sorted_s = sorted(sigmas)
            idx = min(len(sorted_s) - 1, max(0, int(round(0.95 * (len(sorted_s) - 1)))))
            p95 = sorted_s[idx]
        elif sigmas:
            p95 = sigmas[0]
        else:
            p95 = 0.5
        return {
            "article": 9,
            "title": "Risk management system (operator-facing stub)",
            "sigma_mean": round(float(statistics.mean(sigmas)) if sigmas else 0.5, 6),
            "sigma_p95": round(float(p95), 6),
            "abstention_rate": round(abst / n, 6),
            "known_limit": "Kernel is a lab/silicon σ interrupt — measured AUROC lives in repro bundles, not here.",
            "deployment_context": dict(deployment),
        }

    @staticmethod
    def art11_technical_doc(gate: Any, model: Mapping[str, Any], probes: Sequence[Mapping[str, Any]]) -> Dict[str, Any]:
        """Minimum index for a technical file; extend with SoTA model cards / data sheets."""
        del gate
        return {
            "article": 11,
            "architecture": {
                "sigma_gate": "Creation OS σ-gate (Python + optional C kernel)",
                "cascade": "L1–L6 lab hooks when wired",
            },
            "training_data": model.get("training_data_summary", "Operator-supplied"),
            "evaluation": list(probes),
            "limitations": model.get("limitations", ["No on-API headline benchmark claims."]),
        }

    @staticmethod
    def art12_record_keeping(audit_log: Sequence[Mapping[str, Any]]) -> Dict[str, Any]:
        """Summarise append-only rows (hash chain is operator responsibility)."""
        n = len(audit_log)
        verdicts = [str(r.get("verdict", "")).upper() for r in audit_log]
        return {
            "article": 12,
            "records": n,
            "verdict_histogram": {
                "ACCEPT": verdicts.count("ACCEPT"),
                "RETHINK": verdicts.count("RETHINK"),
                "ABSTAIN": verdicts.count("ABSTAIN"),
            },
            "export_note": "Immutability / retention: pair with cos.governance / storage tier.",
        }

    def art13_transparency(self, gate: Any) -> Dict[str, Any]:
        g = gate or self.gate
        ta = float(getattr(g, "threshold_accept", getattr(g, "tau_accept", 0.35)))
        tb = float(getattr(g, "threshold_abstain", getattr(g, "tau_abstain", 0.75)))
        return {
            "article": 13,
            "deployer_summary": (
                f"This deployment uses Creation OS σ-gate with accept τ≈{ta:.3f}, abstain τ≈{tb:.3f}. "
                "Cascade tiers L1–L6 are optional integrator paths."
            ),
            "human_readable": True,
        }

    @staticmethod
    def art14_human_oversight(gate: Any) -> Dict[str, Any]:
        del gate
        return {
            "article": 14,
            "RETHINK": "Route to human review before downstream action or user-facing certainty.",
            "ABSTAIN": "Block automated harm paths; require human decision or retrieval.",
            "ACCEPT": "Low σ — automated path may proceed per policy (still subject to domain risk controls).",
        }

    @staticmethod
    def art15_accuracy(bench_results: Mapping[str, Any]) -> Dict[str, Any]:
        """Pull AUROC/ECE/SNR if present; always list evidence-ladder **negative** quota."""
        negs = bench_results.get("evidence_ladder_negatives")
        if negs is None:
            negs = bench_results.get("negatives_fraction")
        return {
            "article": 15,
            "AUROC": bench_results.get("AUROC"),
            "ECE": bench_results.get("ECE"),
            "SNR": bench_results.get("SNR"),
            "evidence_ladder_negatives_documented": negs is not None,
            "evidence_ladder_negatives": negs,
            "disclaimer": "Populate from archived harness JSON; do not fabricate headline metrics.",
        }

    @staticmethod
    def art50_transparency_mark(response: str, gate: Any) -> str:
        del gate
        blob = {
            "ai_disclosure": "machine_generated_assisted",
            "trace_format": "creation_os_sigma_v1",
            "text_preview": str(response)[:120],
        }
        return json.dumps(blob, sort_keys=True, ensure_ascii=False)

    def art86_right_to_explanation(
        self,
        prompt: str,
        response: str,
        sigma: float,
        verdict: str,
        *,
        explainer: Any = None,
    ) -> Dict[str, Any]:
        from cos.explain import SigmaExplain

        ex = explainer or SigmaExplain(self.gate)
        return {
            "article": 86,
            "explanation": ex.explain(str(prompt), str(response), float(sigma), str(verdict)),
        }

    def full_report(self, period: Mapping[str, Any]) -> Dict[str, Any]:
        """Bundle article stubs for one reporting window."""
        samples = period.get("sample_pairs", [])
        deployment = {"environment": period.get("environment", "lab"), "sample_pairs": samples}
        bench = period.get("bench_results") or {}
        audit = period.get("audit_log") or []
        return {
            "generated_at": datetime.now(timezone.utc).isoformat(),
            "deadline": self.deadline_check(),
            "disclaimer": "Not legal advice; not a conformity certificate.",
            "art9": self.art9_risk_management(self.gate, deployment),
            "art11": self.art11_technical_doc(self.gate, period.get("model", {}), period.get("probes", [])),
            "art12": self.art12_record_keeping(audit),
            "art13": self.art13_transparency(self.gate),
            "art14": self.art14_human_oversight(self.gate),
            "art15": self.art15_accuracy(bench),
            "art50": {"transparency_mark_example": self.art50_transparency_mark(str(period.get("mark_example", "")), self.gate)},
            "art86": self.art86_right_to_explanation(
                str(period.get("explain_prompt", "example")),
                str(period.get("explain_response", "example")),
                float(period.get("explain_sigma", 0.5)),
                str(period.get("explain_verdict", "RETHINK")),
            ),
            "art50_note": "Emit machine-readable disclosure strings where required; example only above.",
        }
