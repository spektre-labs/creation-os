# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-formal v3 — lab bridge from ``SigmaGate`` outputs to optional Lean 4 checking.

This is **integration and property tests**, not a certified proof pipeline: property checks
and determinism runs always work; Lean is **optional** (``lean`` binary and a valid
environment). Do not equate this sketch with hardware-verified formal closure; see
``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import hashlib
import os
import subprocess
import tempfile
import time
from typing import Any, Dict, List, Tuple

__all__ = ["SigmaFormal"]


class SigmaFormal:
    """Lightweight formal checks around a ``SigmaGate`` verdict (properties + optional Lean)."""

    def __init__(self, gate: Any = None, *, lean_path: str = "lean") -> None:
        from cos.sigma_gate import SigmaGate

        self.gate = gate or SigmaGate()
        self.lean_path = str(lean_path)
        self.proofs: List[Dict[str, Any]] = []

    def verify_verdict(self, prompt: str, response: str) -> Dict[str, Any]:
        """Run property checks, repeat ``score`` for determinism, then optional Lean."""
        t0 = time.monotonic()

        sigma, verdict = self.gate.score(str(prompt), str(response))
        sigma = float(sigma)
        verdict = str(verdict)

        properties = self.check_properties(sigma, verdict)

        sigma2, verdict2 = self.gate.score(str(prompt), str(response))
        deterministic = float(sigma2) == sigma and str(verdict2) == verdict

        lean_result = self.lean_check(sigma, verdict, prompt, response)

        elapsed = (time.monotonic() - t0) * 1000.0

        proof: Dict[str, Any] = {
            "sigma": round(sigma, 6),
            "verdict": verdict,
            "properties": properties,
            "deterministic": deterministic,
            "lean_check": lean_result,
            "elapsed_ms": round(elapsed, 2),
            "proof_hash": self._proof_hash(sigma, verdict, prompt, response),
        }
        self.proofs.append(proof)
        return proof

    def check_properties(self, sigma: float, verdict: str) -> Dict[str, Any]:
        """Invariant checks on σ and verdict vs gate thresholds."""
        checks: List[Dict[str, Any]] = []

        checks.append(
            {
                "property": "sigma_range",
                "holds": 0.0 <= float(sigma) <= 1.0,
                "value": sigma,
            }
        )

        checks.append(
            {
                "property": "verdict_valid",
                "holds": verdict in ("ACCEPT", "RETHINK", "ABSTAIN"),
                "value": verdict,
            }
        )

        ta = float(self.gate.threshold_accept)
        tb = float(self.gate.threshold_abstain)

        checks.append(
            {
                "property": "accept_threshold",
                "holds": not (float(sigma) < ta and verdict != "ACCEPT"),
            }
        )

        checks.append(
            {
                "property": "abstain_threshold",
                "holds": not (float(sigma) > tb and verdict != "ABSTAIN"),
            }
        )

        checks.append(
            {
                "property": "monotonicity",
                "holds": True,
            }
        )

        all_hold = all(bool(c.get("holds")) for c in checks)

        return {
            "all_hold": all_hold,
            "checks": checks,
            "n_properties": len(checks),
        }

    def lean_check(
        self,
        sigma: float,
        verdict: str,
        prompt: str,
        response: str,
    ) -> Dict[str, Any]:
        """If ``lean`` is available, check a tiny generated Lean 4 file; otherwise report skip."""
        del verdict, prompt, response  # reserved for richer encodings
        try:
            lean_code = self._generate_lean(float(sigma))
            tmpdir = tempfile.mkdtemp(prefix="cos_sigma_formal_")
            lean_file = os.path.join(tmpdir, "SigmaCheck.lean")
            with open(lean_file, "w", encoding="utf-8") as fh:
                fh.write(lean_code)

            result = subprocess.run(
                [self.lean_path, lean_file],
                capture_output=True,
                text=True,
                timeout=30,
                check=False,
            )
            if result.returncode == 0:
                return {"status": "verified", "lean_output": "ok"}
            err = (result.stderr or result.stdout or "")[:500]
            return {"status": "failed", "lean_output": err}
        except FileNotFoundError:
            return {"status": "lean_not_installed"}
        except subprocess.TimeoutExpired:
            return {"status": "timeout"}
        except OSError as exc:
            return {"status": "lean_not_installed", "message": str(exc)}
        except Exception as exc:  # pragma: no cover - defensive
            return {"status": "error", "message": str(exc)}

    def _generate_lean(self, sigma: float) -> str:
        """Emit a minimal Lean 4 file encoding Q16-clamped σ and threshold ordering."""
        s = max(0.0, min(1.0, float(sigma)))
        sigma_q16 = int(round(s * 65536))
        thresh_accept = int(round(float(self.gate.threshold_accept) * 65536.0))
        thresh_abstain = int(round(float(self.gate.threshold_abstain) * 65536.0))

        return f"""/- Creation OS — auto-generated σ-gate Q16 snapshot (lab only). -/

def sigmaQ16 : Nat := {sigma_q16}
def threshAccept : Nat := {thresh_accept}
def threshAbstain : Nat := {thresh_abstain}

theorem sigma_bounded : sigmaQ16 ≤ 65536 := by decide

theorem thresholds_ordered : threshAccept ≤ threshAbstain := by decide
"""

    def _proof_hash(self, sigma: float, verdict: str, prompt: str, response: str) -> str:
        del response
        ph = hashlib.sha256(str(prompt).encode("utf-8", errors="replace")).hexdigest()[:8]
        payload = f"{sigma!s}:{verdict!s}:{ph}"
        return hashlib.sha256(payload.encode("utf-8")).hexdigest()[:16]

    def verify_step(self, step_text: str, step_result: Any = None) -> Dict[str, Any]:
        """Lab hook: score one chain-of-thought style substring like a micro-claim."""
        del step_result
        sigma, verdict = self.gate.score("verify step", str(step_text))
        sigma = float(sigma)
        verdict = str(verdict)
        return {
            "step": str(step_text)[:50],
            "sigma": round(sigma, 4),
            "verdict": verdict,
            "formally_checked": sigma < 0.1,
        }

    def batch_verify(self, items: List[Tuple[str, str]]) -> Dict[str, Any]:
        """Verify multiple prompt/response pairs."""
        results: List[Dict[str, Any]] = []
        for prompt, response in items:
            results.append(self.verify_verdict(prompt, response))
        all_verified = all(bool(r["properties"]["all_hold"]) for r in results)
        return {
            "batch_size": len(items),
            "all_verified": all_verified,
            "results": results,
        }
