# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-governance — append-only audit, policy checks, compliance summaries (lab).

Does **not** assert formal correctness: pair critical claims with your proof stack
(e.g. Lean) out-of-band per ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import hashlib
import time
from typing import Any, Dict, List, Mapping, Optional, Sequence

__all__ = ["SigmaGovernance"]


class SigmaGovernance:
    """Audit log, rule engine, compliance stub, retention, explanation pointers."""

    def __init__(self, gate: Any = None, *, model_version: str = "lab") -> None:
        from cos.sigma_gate import SigmaGate

        self.gate = gate or SigmaGate()
        self.model_version = str(model_version)
        self._log: List[Dict[str, Any]] = []
        self._retention_days = 90

    def audit_log(
        self,
        *,
        sigma: float,
        verdict: str,
        prompt_hash: str,
        response_hash: str,
        extra: Optional[Mapping[str, Any]] = None,
    ) -> Dict[str, Any]:
        """Append one audit row (public API is append-only)."""
        row = {
            "ts": time.time(),
            "sigma": round(float(sigma), 6),
            "verdict": str(verdict).upper(),
            "input_prompt_hash": str(prompt_hash)[:64],
            "input_response_hash": str(response_hash)[:64],
            "model_version": self.model_version,
            **(dict(extra) if extra else {}),
        }
        row["row_hash"] = _row_digest(row)
        self._log.append(row)
        return dict(row)

    def immutable_snapshot(self) -> Sequence[Dict[str, Any]]:
        """Copy of log (treat as immutable by convention)."""
        return tuple(dict(r) for r in self._log)

    def retention_policy(self, *, days: Optional[int] = None) -> Dict[str, Any]:
        if days is not None:
            self._retention_days = int(max(1, days))
        return {"retention_days": self._retention_days}

    def policy_engine(self, rules: Mapping[str, Any], action: Mapping[str, Any]) -> Dict[str, Any]:
        """Evaluate simple per-user / tool / domain / hour rules."""
        violations: List[str] = []
        uid = str(action.get("user", ""))
        tool = str(action.get("tool", ""))
        domain = str(action.get("domain", ""))
        hour = int(time.localtime().tm_hour)

        denied_users = set(map(str, rules.get("deny_users") or []))
        if uid and uid in denied_users:
            violations.append("user_denied")
        deny_tools = set(map(str, rules.get("deny_tools") or []))
        if tool and tool in deny_tools:
            violations.append("tool_denied")
        deny_domains = set(map(str, rules.get("deny_domains") or []))
        if domain and domain in deny_domains:
            violations.append("domain_denied")
        quiet = rules.get("quiet_hours") or []
        if isinstance(quiet, (list, tuple)) and len(quiet) == 2:
            lo, hi = int(quiet[0]), int(quiet[1])
            if lo <= hour < hi:
                violations.append("quiet_hours")

        allowed = not violations
        return {"allowed": allowed, "violations": violations}

    def compliance_report(self, period_events: Sequence[Mapping[str, Any]]) -> Dict[str, Any]:
        """Aggregate ACCEPT/RETHINK/ABSTAIN counts; Art 50-style transparency stub."""
        acc = ret = abst = 0
        sigmas: List[float] = []
        incidents = 0
        for ev in period_events:
            v = str(ev.get("verdict", "")).upper()
            if v == "ACCEPT":
                acc += 1
            elif v == "RETHINK":
                ret += 1
            elif v == "ABSTAIN":
                abst += 1
            if "sigma" in ev:
                sigmas.append(float(ev["sigma"]))
            if ev.get("incident"):
                incidents += 1
        n = max(len(period_events), 1)
        return {
            "article50_stub": True,
            "counts": {"ACCEPT": acc, "RETHINK": ret, "ABSTAIN": abst, "total": len(period_events)},
            "avg_sigma": round(sum(sigmas) / max(len(sigmas), 1), 6),
            "drift_proxy": round((ret + abst) / n, 6),
            "incidents": incidents,
            "note": "Illustrative transparency report; legal interpretation is out of scope here.",
        }

    def right_to_explanation(self, verdict: str, sigma: float, *, prompt_preview: str = "") -> Dict[str, Any]:
        """Pointer to :mod:`cos.explain` for human-readable rationale (no raw prompt storage)."""
        del prompt_preview
        return {
            "verdict": str(verdict),
            "sigma": float(sigma),
            "explain_module": "cos.explain",
            "endpoint": "/v1/explain",
            "user_message": "Request a natural-language explanation via SigmaExplain.explain(...).",
        }


def _row_digest(row: Mapping[str, Any]) -> str:
    keys = ("ts", "sigma", "verdict", "input_prompt_hash", "input_response_hash", "model_version")
    blob = "|".join(str(row.get(k, "")) for k in keys)
    return hashlib.sha256(blob.encode()).hexdigest()[:16]
