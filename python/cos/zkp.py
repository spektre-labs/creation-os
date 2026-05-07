# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Lab verifiable σ-score receipts via SHA-256 commitments (L7).

Binds (prompt digest, response digest, σ, verdict, gate version) without claiming a
succinct zk-SNARK (see EZKL / Halo2 class tools for that trajectory). This module is
**not** a proof of correct model inference—only **integrity of the logged score payload**.

**NOT AGI ACHIEVED** — see ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import hashlib
import json
import time
from typing import Any, Dict, List, Sequence

__all__ = ["SigmaCommitment", "SigmaConstitution"]


class SigmaCommitment:
    """Commit to a σ-gate outcome; verify by recomputing the hash over the stored payload."""

    def __init__(self) -> None:
        self.commitments: List[Dict[str, Any]] = []

    @staticmethod
    def _hash(data: str) -> str:
        return hashlib.sha256(data.encode("utf-8")).hexdigest()

    def commit(
        self,
        prompt: str,
        response: str,
        sigma: float,
        verdict: Any,
        gate_version: str = "1.0.0",
    ) -> Dict[str, Any]:
        ts = time.time()
        payload = json.dumps(
            {
                "prompt_hash": self._hash(str(prompt)),
                "response_hash": self._hash(str(response)),
                "sigma": round(float(sigma), 4),
                "verdict": str(verdict),
                "gate_version": str(gate_version),
                "timestamp": ts,
            },
            sort_keys=True,
        )
        commitment_hash = self._hash(payload)
        record: Dict[str, Any] = {
            "commitment": commitment_hash,
            "sigma": round(float(sigma), 4),
            "verdict": str(verdict),
            "gate_version": str(gate_version),
            "timestamp": ts,
            "payload": payload,
        }
        self.commitments.append(record)
        return record

    def verify(self, record: Dict[str, Any]) -> bool:
        payload = record.get("payload")
        exp = record.get("commitment")
        if not isinstance(payload, str) or not isinstance(exp, str):
            return False
        return self._hash(payload) == exp

    def proof_receipt(self, record: Dict[str, Any]) -> str:
        return (
            f"=== σ-GATE PROOF RECEIPT (lab commitment) ===\n"
            f"Commitment: {record['commitment'][:16]}...\n"
            f"σ: {record['sigma']}\n"
            f"Verdict: {record['verdict']}\n"
            f"Gate: v{record['gate_version']}\n"
            f"Timestamp: {record['timestamp']}\n"
            f"Verified: {self.verify(record)}\n"
            f"============================================\n"
        )

    def batch_verify(self) -> Dict[str, Any]:
        results: List[Dict[str, Any]] = []
        for c in self.commitments:
            results.append(
                {
                    "commitment": str(c.get("commitment", ""))[:16],
                    "verified": self.verify(c),
                }
            )
        all_valid = all(r["verified"] for r in results)
        return {"total": len(results), "all_valid": all_valid, "results": results}


class SigmaConstitution:
    """Declarative rules checked against a gate instance (lab; not legal/compliance certification)."""

    CONSTITUTION: Sequence[str] = (
        "σ ∈ [0, 1] always",
        "threshold_accept < threshold_abstain always",
        "Evidence ladder includes negatives always",
        "Human decides on irreversible actions always",
        "sigma_gate.h does not change",
        "NOT AGI ACHIEVED until proven otherwise",
        "No silent failures",
        "No hidden thresholds",
        "No data exfiltration",
        "SCSL-1.0 OR AGPL-3.0-only",
    )

    def __init__(self) -> None:
        self.hash = self._compute_hash()

    def _compute_hash(self) -> str:
        content = "\n".join(str(x) for x in self.CONSTITUTION)
        return hashlib.sha256(content.encode("utf-8")).hexdigest()

    def check(self, gate: Any) -> Dict[str, Any]:
        violations: List[str] = []
        try:
            test_sigma, _verdict = gate.score("test", "test")
            if not (0.0 <= float(test_sigma) <= 1.0):
                violations.append("σ not bounded [0,1]")
        except Exception:  # noqa: BLE001 — constitution probe is best-effort
            violations.append("gate.score probe failed")

        ta = float(getattr(gate, "threshold_accept", 0.0))
        tb = float(getattr(gate, "threshold_abstain", 1.0))
        if ta >= tb:
            violations.append("threshold_accept >= threshold_abstain")

        return {
            "compliant": len(violations) == 0,
            "violations": violations,
            "constitution_hash": self.hash,
            "rules": len(self.CONSTITUTION),
        }

    def tampered(self) -> bool:
        return self._compute_hash() != self.hash
