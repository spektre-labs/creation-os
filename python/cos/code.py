# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-code — code scoring: syntax, heuristics, security patterns, :class:`cos.sigma_gate.SigmaGate` blend.

Does **not** replace a SAST product; flags obvious risky idioms only."""
from __future__ import annotations

import ast
import re
from typing import Any, Dict, List

__all__ = ["SigmaCode"]


class SigmaCode:
    """Parse Python when ``language`` is ``python``; other languages return soft checks only."""

    _RISK_PATTERNS = (
        (r"\beval\s*\(", "eval_call"),
        (r"\bexec\s*\(", "exec_call"),
        (r"pickle\.loads", "pickle_loads"),
        (r"os\.system\s*\(", "os_system"),
        (r"subprocess\.", "subprocess_shell_risk"),
        (r"(?i)select\s+.*\bfrom\b", "sql_string"),
        (r"\.\./|\.\.\\", "path_traversal"),
    )

    def score_code(self, prompt: str, generated_code: str, gate: Any) -> Dict[str, Any]:
        syn = self.syntax_check(generated_code, "python")
        sec = self.security_scan(generated_code)
        st = self.static_analysis(generated_code)
        sigma_syn = 0.9 if not syn.get("ok") else 0.15
        sigma_logic = min(1.0, float(st.get("hits", 0)) * 0.12 + float(syn.get("error_count", 0)) * 0.05)
        sigma_sec = min(1.0, len(sec.get("findings", [])) * 0.18)
        blob = f"{prompt}\n```\n{generated_code[:4000]}\n```"
        s_gate, v_gate = gate.score(str(prompt), blob)
        w = 0.25, 0.2, 0.25, 0.3
        comb = w[0] * sigma_syn + w[1] * sigma_logic + w[2] * sigma_sec + w[3] * float(s_gate)
        return {
            "sigma_syntax": round(float(sigma_syn), 6),
            "sigma_logic": round(float(sigma_logic), 6),
            "sigma_security": round(float(sigma_sec), 6),
            "sigma_gate_raw": round(float(s_gate), 6),
            "verdict_gate": str(v_gate),
            "sigma_combined": round(float(comb), 6),
            "syntax": syn,
            "security": sec,
            "static": st,
        }

    @staticmethod
    def syntax_check(code: str, language: str) -> Dict[str, Any]:
        lang = str(language).lower().strip()
        if lang != "python":
            return {"ok": True, "language": lang, "error_count": 0, "note": "non-python skipped"}
        try:
            ast.parse(str(code))
            return {"ok": True, "language": "python", "error_count": 0}
        except SyntaxError as e:
            return {"ok": False, "language": "python", "error_count": 1, "message": str(e)}

    @staticmethod
    def static_analysis(code: str) -> Dict[str, Any]:
        hits = 0
        reasons: List[str] = []
        if len(str(code).splitlines()) > 200 and str(code).count("if ") < 2:
            hits += 1
            reasons.append("long_block_low_branching")
        if str(code).count(";") > 3:
            hits += 1
            reasons.append("multi_statement_lines")
        return {"hits": hits, "reasons": reasons}

    def test_generation(self, code: str, gate: Any) -> Dict[str, Any]:
        syn = self.syntax_check(code, "python")
        tests = "def test_smoke():\n    assert True\n"
        s, v = gate.score("generated_tests", tests)
        red_sigma = 0.35 if syn.get("ok") else 0.75
        return {
            "suggested_tests": tests,
            "sigma_after_tests": round(float(min(s, red_sigma)), 6),
            "verdict": str(v),
        }

    @staticmethod
    def security_scan(code: str) -> Dict[str, Any]:
        findings: List[Dict[str, str]] = []
        src = str(code)
        for pat, tag in SigmaCode._RISK_PATTERNS:
            if re.search(pat, src):
                findings.append({"pattern": pat, "tag": tag})
        return {"findings": findings, "count": len(findings)}

    def sigma_per_function(self, code: str, gate: Any) -> List[Dict[str, Any]]:
        syn = self.syntax_check(code, "python")
        if not syn.get("ok"):
            s, v = gate.score("invalid_python_module", str(code)[:800])
            return [{"function": "<module>", "sigma": round(float(s), 6), "verdict": str(v)}]
        tree = ast.parse(str(code))
        out: List[Dict[str, Any]] = []
        for node in tree.body:
            if isinstance(node, ast.FunctionDef):
                snippet = ast.get_source_segment(str(code), node)
                if snippet is None and hasattr(ast, "unparse"):
                    snippet = ast.unparse(node)
                if snippet is None:
                    snippet = node.name
                s, v = gate.score(f"code_fn:{node.name}", snippet[:2000])
                sec = self.security_scan(snippet)
                bump = min(0.3, len(sec["findings"]) * 0.08)
                out.append(
                    {
                        "function": node.name,
                        "sigma": round(min(1.0, float(s) + bump), 6),
                        "verdict": str(v),
                        "security_hits": sec["count"],
                    }
                )
        return out or [{"function": "<none>", "sigma": 0.5, "verdict": "RETHINK"}]
