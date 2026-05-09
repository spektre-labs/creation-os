# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# Source:        https://github.com/spektre-labs/creation-os-kernel
# Website:       https://spektrelabs.org
# Commercial:    spektre.labs@proton.me
# License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
"""Static project health metrics for the ``cos`` tree (complexity, imports, tests, line count).

This is a lightweight lab report, not a substitute for ``ruff``, ``coverage``, or CI gates.
The aggregate ``health_sigma`` score is a heuristic "doubt" proxy in ``[0, 1]``, analogous
to σ-gate-style bands (see :class:`ProjectHealth.scan`).
"""
from __future__ import annotations

import ast
from pathlib import Path
from typing import Any, Dict, List, Optional, Sequence, Set, Union

PathLike = Union[str, Path]

_DEAD_WHITELIST: Set[str] = {
    "main",
    "score",
    "run",
    "boot",
    "step",
    "process",
    "parse_args",
    "main_cli",
    "cli_main",
}


class ProjectHealth:
    """Scan ``python/cos`` (by default) and summarize maintainability risk heuristics."""

    def __init__(self, root: Optional[PathLike] = None) -> None:
        if root is None:
            self.root = Path(__file__).resolve().parent
        else:
            self.root = Path(root).expanduser().resolve()

    @property
    def _tests_dir(self) -> Path:
        return self.root.parent.parent / "tests"

    def scan(self) -> Dict[str, Any]:
        """Run all heuristics and return a JSON-serializable report."""
        modules = self._list_modules()
        complexity = self._complexity(modules)
        connectivity = self._connectivity(modules)
        dead_code = self._dead_code(modules)
        test_coverage = self._test_mapping(modules)
        line_count = self._line_count(modules)

        issues = (
            complexity["high_complexity"]
            + connectivity["disconnected"]
            + dead_code["dead"]
            + test_coverage["untested"]
        )
        denom = max(len(modules) * 4, 1)
        health_sigma = round(min(1.0, max(0.0, issues / denom)), 4)
        if health_sigma < 0.2:
            verdict = "HEALTHY"
        elif health_sigma < 0.5:
            verdict = "NEEDS ATTENTION"
        else:
            verdict = "CRITICAL"

        return {
            "modules": len(modules),
            "complexity": complexity,
            "connectivity": connectivity,
            "dead_code": dead_code,
            "test_coverage": test_coverage,
            "line_count": line_count,
            "health_sigma": health_sigma,
            "verdict": verdict,
        }

    def _list_modules(self) -> List[Path]:
        out: List[Path] = []
        for f in self.root.rglob("*.py"):
            if "__pycache__" in f.parts:
                continue
            if f.name == "__init__.py":
                continue
            out.append(f)
        return sorted(out)

    def _complexity(self, modules: Sequence[Path]) -> Dict[str, Any]:
        high = 0
        details: List[Dict[str, Any]] = []
        for mod in modules:
            try:
                tree = ast.parse(mod.read_text(encoding="utf-8"))
            except (OSError, SyntaxError, UnicodeDecodeError):
                continue
            for node in ast.walk(tree):
                if not isinstance(node, ast.FunctionDef):
                    continue
                cc = (
                    sum(
                        1
                        for n in ast.walk(node)
                        if isinstance(
                            n,
                            (ast.If, ast.While, ast.For, ast.ExceptHandler, ast.BoolOp),
                        )
                    )
                    + 1
                )
                if cc > 10:
                    high += 1
                    details.append(
                        {"file": mod.name, "function": node.name, "complexity": cc},
                    )
        return {"high_complexity": high, "details": details[:10]}

    def _connectivity(self, modules: Sequence[Path]) -> Dict[str, Any]:
        disconnected: List[str] = []
        for mod in modules:
            try:
                text = mod.read_text(encoding="utf-8")
            except (OSError, UnicodeDecodeError):
                continue
            tl = text.lower()
            mentions_sigma_surface = (
                "sigmagate" in tl
                or "sigma_gate" in tl
                or "sigma" in tl
                or "gate" in tl
            )
            if not mentions_sigma_surface:
                disconnected.append(mod.name)
        return {"disconnected": len(disconnected), "files": disconnected[:20]}

    def _dead_code(self, modules: Sequence[Path]) -> Dict[str, Any]:
        all_defs: Set[str] = set()
        all_calls: Set[str] = set()
        for mod in modules:
            try:
                tree = ast.parse(mod.read_text(encoding="utf-8"))
            except (OSError, SyntaxError, UnicodeDecodeError):
                continue
            for node in ast.walk(tree):
                if isinstance(node, ast.FunctionDef) and not node.name.startswith("_"):
                    all_defs.add(node.name)
                if isinstance(node, ast.Call):
                    f = node.func
                    if isinstance(f, ast.Name):
                        all_calls.add(f.id)
                    elif isinstance(f, ast.Attribute):
                        all_calls.add(f.attr)
        dead = all_defs - all_calls - _DEAD_WHITELIST
        return {"dead": len(dead), "functions": sorted(dead)[:20]}

    def _test_mapping(self, modules: Sequence[Path]) -> Dict[str, Any]:
        td = self._tests_dir
        untested: List[str] = []
        if not td.is_dir():
            return {"untested": len(modules), "files": [m.name for m in modules[:20]]}

        for mod in modules:
            stem = mod.stem
            if not (td / f"test_{stem}.py").is_file():
                untested.append(mod.name)
        return {"untested": len(untested), "files": untested[:20]}

    def _line_count(self, modules: Sequence[Path]) -> Dict[str, int]:
        total = 0
        for mod in modules:
            try:
                total += len(mod.read_text(encoding="utf-8").splitlines())
            except (OSError, UnicodeDecodeError):
                continue
        return {"total_lines": total}
