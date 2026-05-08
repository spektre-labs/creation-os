# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Heuristic **space-grade** *sniffs* on Python sources (AST), plus a TMR majority voter.

This is **not** DO-178C / NASA qualification, **not** a substitute for IV&V, and **not** a
complete Power-of-10 verifier — only a **contributor-facing** reminder aligned with the story in
``docs/SPACE_GRADE.md``. Verdict bands (**ACCEPT** / **RETHINK** / **ABSTAIN**) on
:class:`~cos.sigma_gate.SigmaGate` remain the runtime **safe mode** analogue. **Not AGI achieved.**"""
from __future__ import annotations

import ast
from collections import Counter
from pathlib import Path
from typing import Any, Dict, List, Sequence, Union

from cos.sigma_gate import SigmaGate  # noqa: F401 — σ kernel anchor (sigma_gate.h / Python lite)

__all__ = ["SpaceGradeChecker"]


class SpaceGradeChecker:
    """Static checks inspired by *Power of 10* themes (Python only; heuristic)."""

    def check_file(self, filepath: Union[str, Path]) -> Dict[str, Any]:
        """Return violations for one ``*.py`` file."""
        path = Path(filepath)
        if not path.is_file():
            return {"file": str(path), "error": "not found", "compliant": False, "violations": []}

        source = path.read_text(encoding="utf-8")
        try:
            tree = ast.parse(source)
        except SyntaxError as exc:
            return {
                "file": str(path),
                "error": f"syntax: {exc}",
                "compliant": False,
                "violations": [],
            }

        violations: List[Dict[str, Any]] = []

        for node in ast.walk(tree):
            if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
                end = getattr(node, "end_lineno", None) or node.lineno
                lines = max(1, end - node.lineno + 1)
                if lines > 60:
                    violations.append(
                        {
                            "rule": 4,
                            "description": f"Function '{node.name}' is {lines} lines (max 60)",
                            "line": node.lineno,
                        }
                    )

            if isinstance(node, ast.While):
                has_break = any(isinstance(n, ast.Break) for n in ast.walk(node))
                if not has_break:
                    violations.append(
                        {
                            "rule": 2,
                            "description": "While loop without break — verify bounded iteration",
                            "line": node.lineno,
                        }
                    )

            if isinstance(node, ast.Call):
                func = node.func
                name: str | None = None
                if isinstance(func, ast.Name):
                    name = func.id
                elif isinstance(func, ast.Attribute) and isinstance(func.value, ast.Name):
                    name = func.attr
                if name in ("exec", "eval", "compile"):
                    violations.append(
                        {
                            "rule": 3,
                            "description": f"Dynamic execution: {name}()",
                            "line": node.lineno,
                        }
                    )

        return {
            "file": str(path),
            "violations": violations,
            "compliant": len(violations) == 0,
            "score": max(0, 10 - len(violations)),
        }

    def check_module(self, module_dir: Union[str, Path] = "python/cos") -> Dict[str, Any]:
        """Scan all ``*.py`` under ``module_dir`` (expects repo-style path)."""
        root = Path(module_dir)
        results: List[Dict[str, Any]] = []
        for f in sorted(root.rglob("*.py")):
            if "__pycache__" in f.parts:
                continue
            results.append(self.check_file(f))

        compliant_n = sum(1 for r in results if r.get("compliant"))
        all_violations: List[Dict[str, Any]] = []
        for r in results:
            for v in r.get("violations", []):
                entry = dict(v)
                entry["file"] = r.get("file", "")
                all_violations.append(entry)

        n = max(len(results), 1)
        return {
            "total_files": len(results),
            "compliant": compliant_n,
            "non_compliant": len(results) - compliant_n,
            "compliance_rate": round(compliant_n / n, 4),
            "all_violations": all_violations,
            "results": results,
        }

    def tmr_check(
        self,
        results_a: Sequence[Any],
        results_b: Sequence[Any],
        results_c: Sequence[Any],
    ) -> Dict[str, Any]:
        """Majority vote across three equal-length probe lists (TMR toy)."""
        voted: List[Dict[str, Any]] = []
        for a, b, c in zip(results_a, results_b, results_c):
            values = [a, b, c]
            top = Counter(values).most_common(1)[0]
            vote, agreed = top[0], top[1]
            voted.append(
                {
                    "value": vote,
                    "agreement": agreed,
                    "unanimous": agreed == 3,
                    "corrected": agreed < 3,
                }
            )
        corrections = sum(1 for v in voted if v["corrected"])
        denom = max(len(voted), 1)
        return {
            "voted_results": voted,
            "corrections": corrections,
            "reliability": round(1.0 - corrections / denom, 4),
        }
