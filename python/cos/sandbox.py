# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-sandbox — restricted evaluation + σ before/after hooks (lab).

Not a process sandbox: **single-expression eval** only, empty builtins. For real isolation
use OS containers. See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

from typing import Any, Dict, Optional

__all__ = ["SigmaSandbox"]


class SigmaSandbox:
    """Pre-execution σ, resource metadata, post-execution σ on stringified output."""

    def __init__(self, gate: Any = None) -> None:
        from cos.sigma_gate import SigmaGate

        self.gate = gate or SigmaGate()
        self._last_ok = True
        self._scratch: Dict[str, Any] = {}

    def filesystem_isolation(self) -> Dict[str, Any]:
        return {"host_fs_access": False, "workspace": ":memory:", "note": "Lab evaluator has no FS hooks."}

    def network_isolation(self) -> Dict[str, Any]:
        return {"outbound": False, "whitelist": []}

    def resource_limits(self, *, timeout_s: float = 1.0, memory_mb: int = 64) -> Dict[str, Any]:
        return {"timeout_s": float(timeout_s), "memory_mb": int(memory_mb), "cpu_affinity": None}

    def sigma_before_execute(self, code: str, gate: Optional[Any] = None) -> Dict[str, Any]:
        g = gate or self.gate
        sigma, verdict = g.score("sandbox_code", str(code)[:8000])
        return {"sigma": round(float(sigma), 6), "verdict": str(verdict), "safe_to_proceed": float(sigma) < 0.85}

    def execute_safe(self, code: str, timeout_s: float = 1.0, memory_mb: int = 64) -> Dict[str, Any]:
        del timeout_s, memory_mb  # honored by real sandbox; lab uses eval-only
        pre = self.sigma_before_execute(code)
        if not pre["safe_to_proceed"]:
            self._last_ok = False
            self.rollback_on_failure()
            return {"ok": False, "reason": "sigma_precheck", **pre}
        src = str(code).strip()
        if "\n" in src or ";" in src:
            self._last_ok = False
            self.rollback_on_failure()
            return {"ok": False, "error": "multi_statement_not_allowed"}
        try:
            expr = compile(src, "<sigma_sandbox>", "eval", dont_inherit=True)
            out = eval(expr, {"__builtins__": {}}, {})
            self._scratch["last_result"] = out
            self._last_ok = True
            return {
                "ok": True,
                "result": out,
                "filesystem": self.filesystem_isolation(),
                "network": self.network_isolation(),
            }
        except Exception as e:
            self._last_ok = False
            self.rollback_on_failure()
            return {"ok": False, "error": str(e)}

    def result_sigma(self, output: Any, gate: Optional[Any] = None) -> Dict[str, Any]:
        g = gate or self.gate
        text = str(output)[:8000]
        sigma, verdict = g.score("sandbox_output", text)
        return {"sigma": round(float(sigma), 6), "verdict": str(verdict)}

    def rollback_on_failure(self) -> Dict[str, Any]:
        if not self._last_ok:
            self._scratch.clear()
        return {"cleaned": not self._last_ok}
