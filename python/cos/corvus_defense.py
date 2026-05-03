# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""CORVUS-style defense sketch — multi-probe consensus, weight integrity, canary hallucination.

**Lab only:** “adaptive attacker” claims require harnesses outside this file. Use with
:class:`cos.sigma_red_team.SigmaRedTeam` for red-team categories, not as a certified defense.
See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import secrets
from typing import Any, Callable, Dict, Mapping, Sequence

__all__ = ["SigmaCorvusDefense"]


class SigmaCorvusDefense:
    """Detect probe agreement anomalies, hash drift, and silent probe failure via canaries."""

    def __init__(self, gate: Any = None) -> None:
        from cos.sigma_gate import SigmaGate

        self.gate = gate or SigmaGate()

    def detect_signal_masking(
        self,
        model: Any,
        baseline_model: Any,
        *,
        probe: Callable[[Any], float],
    ) -> Dict[str, Any]:
        """Compare scalar probe statistics; large delta ⇒ investigate (not proof of attack)."""
        try:
            s_now = float(probe(model))
            s_base = float(probe(baseline_model))
        except Exception as e:  # pragma: no cover
            return {"ok": False, "error": str(e), "suspicious": False}
        delta = abs(s_now - s_base)
        return {
            "ok": True,
            "delta": round(delta, 6),
            "current": s_now,
            "baseline": s_base,
            "suspicious": delta > 0.35,
        }

    def multi_signal_consensus(self, probe_scores: Mapping[str, float]) -> Dict[str, Any]:
        """If one probe is very low while others are high ⇒ RETHINK (possible masking)."""
        vals = list(probe_scores.values())
        if not vals:
            return {"verdict": "RETHINK", "reason": "empty"}
        lo, hi = min(vals), max(vals)
        avg = sum(vals) / len(vals)
        outlier_low = [k for k, v in probe_scores.items() if v < avg - 0.35 and hi > 0.55]
        outlier_high = [k for k, v in probe_scores.items() if v > avg + 0.35]
        suspicious = bool(outlier_low and len(outlier_high) >= 1)
        verdict = "RETHINK" if suspicious else "ACCEPT"
        return {
            "verdict": verdict,
            "suspicious": suspicious,
            "spread": round(hi - lo, 6),
            "outlier_low": outlier_low,
        }

    def probe_diversity(self, active_probes: Sequence[str]) -> Dict[str, Any]:
        families = {"sink", "spectral", "icr", "sep", "hide", "entropy", "energy", "lsd"}
        covered = [p for p in active_probes if any(f in p.lower() for f in families)]
        return {"n_probes": len(active_probes), "diverse_labels": covered, "score": min(1.0, len(covered) / 4.0)}

    @staticmethod
    def sigma_integrity_check(model_hash: str, expected_hash: str) -> Dict[str, Any]:
        ok = secrets.compare_digest(model_hash.strip(), expected_hash.strip())
        return {"ok": ok, "match": ok}

    def canary_injection(
        self,
        canary_prompt: str,
        canary_response: str,
        *,
        min_sigma: float = 0.25,
    ) -> Dict[str, Any]:
        """Obvious junk should not read as ultra-low σ; failure suggests compromised probes."""
        sigma, verdict = self.gate.score(canary_prompt, canary_response)
        triggered = float(sigma) < float(min_sigma) and verdict == "ACCEPT"
        return {
            "sigma": float(sigma),
            "verdict": verdict,
            "probe_compromised_suspect": bool(triggered),
            "note": "Canary uses lite gate unless LSD wired; tune min_sigma per stack.",
        }

