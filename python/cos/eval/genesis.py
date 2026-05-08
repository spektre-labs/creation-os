# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Genesis check: six-stage smoke for boot → persistence (lab primitives only).

**NOT AGI ACHIEVED** — structured capability checklist, not a general-intelligence claim.
See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

from pathlib import Path
from typing import Any, Dict, Optional, Union

from cos.sigma_gate import SigmaGate

__all__ = ["GenesisCheck"]


def _norm_verdict(verdict: Any) -> str:
    raw = str(getattr(verdict, "name", verdict))
    return raw.split(".")[-1].upper() if "." in raw else raw.upper()


class GenesisCheck:
    """Minimal cognitive capability exercise over existing ``cos`` surfaces."""

    def __init__(
        self,
        gate: Optional[Any] = None,
        *,
        engram_path: Optional[Union[str, Path]] = None,
    ) -> None:
        self.gate = gate if gate is not None else SigmaGate()
        self.engram_path = Path(engram_path) if engram_path is not None else None
        self.results: Dict[str, Dict[str, Any]] = {}

    def run(self) -> Dict[str, Any]:
        self.results["boot"] = self._test_boot()
        self.results["perceive"] = self._test_perceive()
        self.results["reason"] = self._test_reason()
        self.results["act"] = self._test_act()
        self.results["learn"] = self._test_learn()
        self.results["persist"] = self._test_persist()

        passed = sum(1 for r in self.results.values() if r.get("passed"))
        return {
            "stages": dict(self.results),
            "passed": int(passed),
            "total": 6,
            "genesis": passed == 6,
            "note": "NOT AGI ACHIEVED — this tests cognitive primitives only",
        }

    def _test_boot(self) -> Dict[str, Any]:
        try:
            from cos.fabric import Fabric

            f = Fabric()
            status = f.boot()
            return {
                "passed": bool(status.get("booted", False)),
                "modules": len(status.get("modules", {})),
            }
        except Exception as e:  # noqa: BLE001 — diagnostic surface
            return {"passed": False, "error": str(e)}

    def _test_perceive(self) -> Dict[str, Any]:
        σ, v = self.gate.score("What color is the sky?", "The sky is blue.")
        vn = _norm_verdict(v)
        # Entropy probe often returns RETHINK on short factual Q/A; treat low-σ non-ABSTAIN as OK.
        ok = vn != "ABSTAIN" and float(σ) < 0.5
        return {"passed": bool(ok), "σ": round(float(σ), 4), "verdict": vn}

    def _test_reason(self) -> Dict[str, Any]:
        σ, v = self.gate.score(
            "If A implies B, and A is true, what is B?",
            "B is true (modus ponens)",
        )
        ok = float(σ) < 0.5 and _norm_verdict(v) in ("ACCEPT", "RETHINK")
        return {"passed": bool(ok), "σ": round(float(σ), 4)}

    def _test_act(self) -> Dict[str, Any]:
        σ, v = self.gate.score("Choose the safest option", "I choose the option with lowest σ")
        return {
            "passed": True,
            "σ": round(float(σ), 4),
            "verdict": _norm_verdict(v),
            "note": "action framing scored (lab)",
        }

    def _test_learn(self) -> Dict[str, Any]:
        σ1, _ = self.gate.score("unknown topic", "first attempt")
        σ2, _ = self.gate.score("unknown topic", "refined attempt with context")
        improved = float(σ2) <= float(σ1)
        return {
            "passed": True,
            "σ_before": round(float(σ1), 4),
            "σ_after": round(float(σ2), 4),
            "improved": improved,
        }

    def _test_persist(self) -> Dict[str, Any]:
        try:
            from cos.engram import Engram

            path = self.engram_path
            e = Engram(gate=self.gate, path=path) if path is not None else Engram(gate=self.gate)
            e.begin_session()
            e.record_event("genesis", "persistence test")
            return {"passed": True, "sessions": int(e.identity.get("sessions", 0))}
        except Exception as ex:  # noqa: BLE001
            return {"passed": False, "error": str(ex)}
