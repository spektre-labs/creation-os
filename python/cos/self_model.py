# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Self-model: empirical σ-scored beliefs about capabilities and declared limitations.

Capabilities are updated only via :meth:`assess_capability` (repeated ``test_fn`` runs).
This is a lab integration surface, not a claim of human-level self-awareness.
**NOT AGI ACHIEVED** — see ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

from typing import Any, Callable, Dict, List, Optional, Tuple

__all__ = ["SelfModel"]


def _norm_verdict(v: Any) -> str:
    raw = str(getattr(v, "name", v))
    return raw.split(".")[-1] if "." in raw else raw


class SelfModel:
    """σ-scored record of what the stack claims it can do vs measured probes."""

    def __init__(self, gate: Any = None) -> None:
        from cos.sigma_gate import SigmaGate

        self.gate = gate or SigmaGate()
        self.capabilities: Dict[str, Dict[str, Any]] = {}
        self.limitations: Dict[str, Dict[str, Any]] = {}
        self.state: Dict[str, Any] = {}

    def assess_capability(self, skill: str, test_fn: Callable[[], Any], n_tests: int = 10) -> Dict[str, Any]:
        """Empirically probe *skill* with *n_tests* runs of *test_fn*; σ from gate on each result."""
        n = max(1, int(n_tests))
        successes = 0
        σ_values: List[float] = []
        for _ in range(n):
            try:
                result = test_fn()
                σ, verdict = self.gate.score(f"can I {skill}?", str(result))
                σ_values.append(float(σ))
                if _norm_verdict(verdict) == "ACCEPT":
                    successes += 1
            except Exception:
                σ_values.append(1.0)

        avg_σ = sum(σ_values) / max(len(σ_values), 1)
        verdict = "capable" if avg_σ < 0.3 else "limited" if avg_σ < 0.7 else "incapable"
        self.capabilities[str(skill)] = {
            "σ": round(float(avg_σ), 4),
            "success_rate": round(successes / n, 4),
            "evidence_count": n,
            "verdict": verdict,
        }
        return self.capabilities[str(skill)]

    def declare_limitation(self, domain: str, reason: str) -> Dict[str, Any]:
        """Record an explicit limitation (still σ-scored on prompt/reason text)."""
        σ, _v = self.gate.score(f"limitation in {domain}", str(reason))
        entry = {
            "reason": str(reason),
            "σ": round(float(σ), 4),
            "declared": True,
        }
        self.limitations[str(domain)] = entry
        return entry

    def can_i(self, skill: str) -> Tuple[Optional[bool], float]:
        """Return (known?, σ or default). ``None`` means no capability/limit record."""
        s = str(skill)
        if s in self.capabilities:
            cap = self.capabilities[s]
            return cap["verdict"] == "capable", float(cap["σ"])
        if s in self.limitations:
            return False, float(self.limitations[s]["σ"])
        return None, 0.5

    def accuracy(self) -> float:
        """Rough calibration: declared *verdict* vs observed *success_rate* bands."""
        if not self.capabilities:
            return 0.5
        correct = 0
        for c in self.capabilities.values():
            sr = float(c["success_rate"])
            v = str(c["verdict"])
            ok = (
                (v == "capable" and sr > 0.7)
                or (v == "incapable" and sr < 0.3)
                or (v == "limited" and 0.3 <= sr <= 0.7)
            )
            if ok:
                correct += 1
        return round(correct / len(self.capabilities), 4)

    def report(self) -> Dict[str, Any]:
        return {
            "capabilities": dict(self.capabilities),
            "limitations": dict(self.limitations),
            "n_skills_assessed": len(self.capabilities),
            "n_limitations_declared": len(self.limitations),
            "self_model_accuracy": self.accuracy(),
            "agi_claim": "NOT AGI ACHIEVED",
        }
