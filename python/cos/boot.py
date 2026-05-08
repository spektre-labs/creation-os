# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Creation OS startup sequence: kernel σ-check, engram session, :class:`~cos.fabric.Fabric`,
:class:`~cos.mega.Mega`, and default σ routing cascade.

**NOT AGI ACHIEVED** — orchestration only; measured claims belong in harness bundles per
``docs/CLAIM_DISCIPLINE.md``. The C σ core remains the policy floor."""
from __future__ import annotations

from typing import Any, Dict, Optional

from cos.sigma_gate import SigmaGate

__all__ = ["Boot"]


class Boot:
    """Run a single bounded startup sequence; optional subsystems skip on failure."""

    def __init__(self, gate: Optional[Any] = None) -> None:
        self.gate = gate or SigmaGate()
        self.booted = False
        self.modules: Dict[str, Any] = {}

    def run(self) -> Dict[str, Any]:
        """Execute kernel → identity → fabric → mega → cascade; tolerate partial availability."""
        steps: Dict[str, Dict[str, Any]] = {}

        σ, _v = self.gate.score("boot", "system starting")
        steps["kernel"] = {"σ": round(float(σ), 4), "status": "OK"}

        engram_inst: Optional[Any] = None
        try:
            from cos.engram import Engram

            engram_inst = Engram(gate=self.gate)
            engram_inst.begin_session()
            steps["identity"] = {
                "status": "OK",
                "sessions": int(engram_inst.identity.get("sessions", 0)),
            }
            self.modules["engram"] = engram_inst
        except Exception as ex:  # noqa: BLE001 — boot must continue
            steps["identity"] = {"status": "SKIP", "reason": str(ex)}

        try:
            from cos.fabric import Fabric

            disabled = ["engram"] if engram_inst is not None else []
            fab = Fabric(disabled_modules=disabled)
            fab.gate = self.gate
            status = fab.boot()
            if engram_inst is not None:
                fab._modules["engram"] = engram_inst
            steps["fabric"] = {
                "status": "OK",
                "modules": len(status.get("modules", {})),
            }
            self.modules["fabric"] = fab
        except Exception as ex:  # noqa: BLE001
            steps["fabric"] = {"status": "SKIP", "reason": str(ex)}

        try:
            from cos.mega import Mega

            mega = Mega(gate=self.gate)
            steps["mega"] = {
                "status": "OK",
                "modules": int(mega.status().get("modules_loaded", 0)),
            }
            self.modules["mega"] = mega
        except Exception as ex:  # noqa: BLE001
            steps["mega"] = {"status": "SKIP", "reason": str(ex)}

        try:
            from cos.cascade_router import SigmaCascade

            cascade = SigmaCascade.default_cascade(self.gate)
            steps["cascade"] = {
                "status": "OK",
                "levels": len(cascade.levels),
            }
            self.modules["cascade"] = cascade
        except Exception as ex:  # noqa: BLE001
            steps["cascade"] = {"status": "SKIP", "reason": str(ex)}

        self.booted = True
        loaded = sum(1 for s in steps.values() if s.get("status") == "OK")
        total = len(steps)
        return {
            "booted": True,
            "steps": steps,
            "loaded": loaded,
            "total": total,
            "ready": loaded >= 2,
            "message": (
                f"Creation OS ready. {loaded}/{total} steps OK. σ-gate active."
                if loaded >= 2
                else f"Partial boot. {loaded}/{total} steps OK."
            ),
        }
