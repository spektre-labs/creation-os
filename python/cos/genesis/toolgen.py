# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Runtime tool creation lab: **ast** + substring guards + σ intent/code scoring.

A minimal *skill store* pattern — not a certified sandbox, IDE, or autonomous safety system.
``exec`` is constrained by a small **builtins** allowlist; substring blocks catch obvious footguns
only. Evidence and headline claims stay under ``docs/CLAIM_DISCIPLINE.md``. ``sigma_gate.h`` is
not modified here.
"""
from __future__ import annotations

import ast
import hashlib
from typing import Any, Callable, Dict, List, Optional, Tuple

from cos.sigma_gate import SigmaGate

__all__ = ["SigmaToolGen", "Tool"]

ProblemFn = Callable[[str], Tuple[str, str, str]]


def _verdict_label(verdict: Any) -> str:
    raw = str(getattr(verdict, "name", verdict))
    base = raw.split(".")[-1] if "." in raw else raw
    return base.upper()


_SAFE_BUILTINS: Dict[str, Any] = {
    "__builtins__": {
        "len": len,
        "range": range,
        "int": int,
        "str": str,
        "float": float,
        "bool": bool,
        "min": min,
        "max": max,
        "sum": sum,
        "abs": abs,
        "enumerate": enumerate,
        "zip": zip,
        "isinstance": isinstance,
        "type": type,
    }
}


class Tool:
    """A runtime-registered callable produced from source text (lab)."""

    def __init__(
        self,
        name: str,
        code: str,
        intent: str,
        sigma_at_creation: float,
    ) -> None:
        self.name = str(name)
        self.code = str(code)
        self.intent = str(intent)
        self.sigma_at_creation = float(sigma_at_creation)
        self.usage_count = 0
        self.sigma_history: List[float] = []
        self.hash = hashlib.sha256(self.code.encode("utf-8")).hexdigest()[:12]

    def execute(self, *args: Any, **kwargs: Any) -> Any:
        namespace: Dict[str, Any] = dict(_SAFE_BUILTINS)
        exec(self.code, namespace)  # noqa: S102 — intentional lab execution path
        fn = namespace.get(self.name)
        if fn is not None and callable(fn):
            self.usage_count += 1
            return fn(*args, **kwargs)
        return None


class SigmaToolGen:
    """σ-gated tool registration: parse, blocklist, intent match, store, execute."""

    def __init__(self, gate: Optional[Any] = None) -> None:
        self.gate = gate if gate is not None else SigmaGate()
        self.tools: Dict[str, Tool] = {}
        self.failed: List[Dict[str, Any]] = []
        self.creation_log: List[Dict[str, Any]] = []

    def need_tool(self, problem: str) -> Dict[str, Any]:
        existing = list(self.tools.keys())
        sigma, _ver = self.gate.score(
            f"existing tools: {existing}",
            f"problem: {problem}",
        )
        sigma = float(sigma)
        need = sigma > 0.5
        return {
            "need": need,
            "σ": round(sigma, 4),
            "existing_tools": len(existing),
            "recommendation": "create new tool" if need else "existing tools sufficient",
        }

    def create(self, name: str, code: str, intent: str) -> Dict[str, Any]:
        nm = str(name)
        try:
            ast.parse(code)
        except SyntaxError as e:
            self.failed.append({"name": nm, "reason": f"syntax: {e}"})
            return {"created": False, "reason": f"SyntaxError: {e}"}

        dangerous = (
            "os.system",
            "subprocess",
            "eval(",
            "exec(",
            "__import__",
            "open(",
            "shutil.rmtree",
        )
        for d in dangerous:
            if d in code:
                self.failed.append({"name": nm, "reason": f"unsafe: {d}"})
                return {"created": False, "reason": f"BLOCKED: {d}"}

        sigma, verdict = self.gate.score(intent, code)
        sigma = float(sigma)
        if _verdict_label(verdict) == "ABSTAIN":
            self.failed.append({"name": nm, "σ": sigma, "reason": "σ too high"})
            return {"created": False, "σ": round(sigma, 4), "reason": "ABSTAIN"}

        tool = Tool(nm, code, intent, sigma)
        self.tools[nm] = tool
        self.creation_log.append(
            {
                "name": nm,
                "intent": intent,
                "σ": round(sigma, 4),
                "hash": tool.hash,
            }
        )

        return {
            "created": True,
            "name": nm,
            "σ": round(sigma, 4),
            "hash": tool.hash,
            "tools_total": len(self.tools),
        }

    def use(self, name: str, *args: Any, **kwargs: Any) -> Dict[str, Any]:
        nm = str(name)
        if nm not in self.tools:
            return {"error": f"tool '{nm}' not found"}

        tool = self.tools[nm]
        try:
            result = tool.execute(*args, **kwargs)
            sigma, _ver = self.gate.score(tool.intent, str(result))
            sigma = float(sigma)
            tool.sigma_history.append(sigma)
            return {
                "result": result,
                "σ": round(sigma, 4),
                "tool": nm,
                "usage": tool.usage_count,
            }
        except Exception as e:
            return {"error": str(e), "tool": nm}

    def find_tool(self, problem: str) -> Optional[Dict[str, Any]]:
        if not self.tools:
            return None

        best: Optional[str] = None
        best_sigma = 1.0
        for tname, tool in self.tools.items():
            sigma, _ = self.gate.score(str(problem), tool.intent)
            sigma = float(sigma)
            if sigma < best_sigma:
                best_sigma = sigma
                best = tname

        if best_sigma < 0.5:
            return {"tool": best, "σ": round(best_sigma, 4)}
        return None

    def auto_solve(
        self,
        problem: str,
        generate_fn: ProblemFn,
    ) -> Dict[str, Any]:
        existing = self.find_tool(problem)
        if existing:
            used = self.use(existing["tool"])
            return {**used, "source": "existing"}

        name, code, intent = generate_fn(problem)
        create_result = self.create(name, code, intent)
        if not create_result.get("created"):
            return {**create_result, "source": "creation_failed"}

        use_result = self.use(name)
        return {**use_result, "source": "newly_created"}

    def evolve_tool(
        self,
        name: str,
        improved_code: str,
        improve_fn: Any = None,
    ) -> Dict[str, Any]:
        del improve_fn  # reserved API hook; not used in this lab shell
        nm = str(name)
        if nm not in self.tools:
            return {"error": "tool not found"}

        old_tool = self.tools[nm]
        sigma_old = float(old_tool.sigma_at_creation)

        try:
            ast.parse(improved_code)
        except SyntaxError as e:
            return {"evolved": False, "reason": f"syntax: {e}"}

        dangerous = (
            "os.system",
            "subprocess",
            "eval(",
            "exec(",
            "__import__",
            "open(",
            "shutil.rmtree",
        )
        for d in dangerous:
            if d in improved_code:
                return {"evolved": False, "reason": f"BLOCKED: {d}"}

        sigma_new, _ver = self.gate.score(old_tool.intent, improved_code)
        sigma_new = float(sigma_new)

        if sigma_new <= sigma_old:
            new_tool = Tool(nm, improved_code, old_tool.intent, sigma_new)
            new_tool.usage_count = old_tool.usage_count
            self.tools[nm] = new_tool
            return {
                "evolved": True,
                "name": nm,
                "σ_old": round(sigma_old, 4),
                "σ_new": round(sigma_new, 4),
            }

        return {
            "evolved": False,
            "reason": "σ regression",
            "σ_old": round(sigma_old, 4),
            "σ_new": round(sigma_new, 4),
        }

    def capability_surface(self) -> Dict[str, Any]:
        return {
            "tools": len(self.tools),
            "capabilities": [
                {
                    "name": t.name,
                    "intent": t.intent,
                    "usage": t.usage_count,
                    "σ": round(float(t.sigma_at_creation), 4),
                }
                for t in self.tools.values()
            ],
            "failed_creations": len(self.failed),
            "surface_area": len(self.tools),
            "growing": len(self.creation_log) > 0,
        }
