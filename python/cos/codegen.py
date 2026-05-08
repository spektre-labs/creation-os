# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# Source:        https://github.com/spektre-labs/creation-os-kernel
# Website:       https://spektrelabs.org
# Commercial:    spektre.labs@proton.me
# License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
"""σ-gated code generation helpers (lab): syntax parse, gate score, optional size heuristics.

Each candidate chunk can be scored with :class:`~cos.sigma_gate.SigmaGate` as a **cheap**
prompt–response check (intent vs artifact). This is **not** a substitute for execution tests,
types, or human review; see ``docs/CLAIM_DISCIPLINE.md`` for evidence scope.

**NOT AGI ACHIEVED** — bounded scaffolding only; combine with ``cos.evolve`` only inside lab envelopes.
"""
from __future__ import annotations

import ast
from typing import Any, Callable, Dict, List

from cos.sigma_gate import ABSTAIN, ACCEPT, SigmaGate

__all__ = ["SigmaCodeGen"]

GenerateFn = Callable[[str], str]
ImproveFn = Callable[[str, str, Dict[str, Any]], str]


def _verdict_str(verdict: Any) -> str:
    raw = str(getattr(verdict, "name", verdict))
    base = raw.split(".")[-1] if "." in raw else raw
    return base.upper()


class SigmaCodeGen:
    """Generate or refine code artifacts with σ checks and AST sanity hooks."""

    def __init__(self, gate: Any = None) -> None:
        self.gate = gate if gate is not None else SigmaGate()
        self.generated: List[Dict[str, Any]] = []
        self.rejected: List[Dict[str, Any]] = []

    def validate_code(self, code: str, intent: str = "") -> Dict[str, Any]:
        """Parse Python, score ``intent`` vs ``code``, flag long functions (lab heuristic)."""
        try:
            tree = ast.parse(code)
        except SyntaxError as exc:
            row = {
                "valid": False,
                "σ": 1.0,
                "verdict": ABSTAIN,
                "syntax_ok": False,
                "error": f"SyntaxError: {exc}",
                "functions": 0,
                "long_functions": 0,
                "nasa_compliant": False,
            }
            self.rejected.append(row)
            return row

        intent_key = intent or "code quality"
        σ, verdict = self.gate.score(intent_key, code)
        σ = float(σ)
        vs = _verdict_str(verdict).upper()
        if vs not in ("ACCEPT", "RETHINK", "ABSTAIN"):
            vs = str(verdict)

        functions = [n for n in ast.walk(tree) if isinstance(n, ast.FunctionDef)]
        long_fns: List[ast.FunctionDef] = []
        for f in functions:
            end = getattr(f, "end_lineno", None)
            start = getattr(f, "lineno", 1)
            if end is not None and end - start > 60:
                long_fns.append(f)

        valid = bool(vs != ABSTAIN)
        result: Dict[str, Any] = {
            "valid": valid,
            "σ": round(σ, 4),
            "verdict": vs,
            "syntax_ok": True,
            "functions": len(functions),
            "long_functions": len(long_fns),
            "nasa_compliant": len(long_fns) == 0,
        }

        if valid:
            self.generated.append(result)
        else:
            self.rejected.append(result)
        return result

    def validate_test(self, test_code: str, source_code: str) -> Dict[str, Any]:
        """σ prompt pairs test source with its tests (relevance proxy, not coverage proof)."""
        prompt = f"test coverage for: {source_code[:200]}"
        σ, verdict = self.gate.score(prompt, test_code)
        σ = float(σ)
        vs = _verdict_str(verdict).upper()
        return {
            "σ": round(σ, 4),
            "verdict": vs,
            "meaningful": vs == ACCEPT,
        }

    def validate_prose(self, text: str, intent: str) -> Dict[str, Any]:
        """Non-Python text (e.g. doc blobs): σ only, no ``ast`` parse."""
        σ, verdict = self.gate.score(intent, text)
        vs = _verdict_str(verdict).upper()
        return {"σ": round(float(σ), 4), "verdict": vs, "chars": len(text)}

    def generate_module(self, spec: str, generate_fn: GenerateFn) -> Dict[str, Any]:
        """Synthesize code, tests, and prose docs; σ each piece (tests as Python)."""
        code = generate_fn(f"write code for: {spec}")
        results: Dict[str, Any] = {}
        results["code"] = self.validate_code(code, spec)

        tests = generate_fn(f"write tests for: {spec}\n\nCode:\n{code}")
        results["tests"] = self.validate_code(tests, f"tests for {spec}")
        results["test_relevance"] = self.validate_test(tests, code)

        docs = generate_fn(f"write docstring for: {spec}\n\nCode:\n{code}")
        results["docs"] = self.validate_prose(docs, f"documentation for {spec}")

        all_σ = [
            float(results["code"]["σ"]),
            float(results["tests"]["σ"]),
            float(results["test_relevance"]["σ"]),
        ]
        avg_σ = sum(all_σ) / len(all_σ)
        pieces_valid = sum(1 for r in (results["code"], results["tests"]) if r.get("valid"))
        results["module_verdict"] = {
            "avg_σ": round(avg_σ, 4),
            "accept": avg_σ < 0.4,
            "pieces_valid": pieces_valid,
        }
        return results

    def recursive_improve(
        self,
        code: str,
        intent: str,
        improve_fn: ImproveFn,
        max_rounds: int = 5,
    ) -> Dict[str, Any]:
        """Edit–score loop; keep the lowest-σ snapshot; stop on ACCEPT verdict."""
        current = str(code)
        best_code = current
        best_σ = float("inf")
        max_rounds = max(1, int(max_rounds))

        for round_n in range(max_rounds):
            result = self.validate_code(current, intent)
            last_σ = float(result["σ"])
            if last_σ < best_σ:
                best_σ = last_σ
                best_code = current
            if str(result["verdict"]) == ACCEPT:
                return {
                    "code": current,
                    "rounds": round_n + 1,
                    "σ": last_σ,
                    "verdict": ACCEPT,
                }
            current = improve_fn(current, intent, result)

        return {
            "code": best_code,
            "rounds": max_rounds,
            "σ": round(float(best_σ), 4),
            "verdict": "RETHINK",
            "note": "max rounds reached",
        }

    def stats(self) -> Dict[str, Any]:
        total = len(self.generated) + len(self.rejected)
        return {
            "generated": len(self.generated),
            "rejected": len(self.rejected),
            "acceptance_rate": round(len(self.generated) / max(total, 1), 4),
            "avg_σ_accepted": round(
                sum(float(g["σ"]) for g in self.generated) / max(len(self.generated), 1),
                4,
            )
            if self.generated
            else 0.0,
        }
