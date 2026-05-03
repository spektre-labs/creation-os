# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-table — tabular output checks (schema, outliers, SQL text leg).

For production validation use your DB engine + typed schemas. See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import json
import re
import statistics
from typing import Any, Dict, List, Mapping, Sequence, Union

__all__ = ["SigmaTable"]

TableLike = Union[str, Sequence[Mapping[str, Any]]]


class SigmaTable:
    """Score markdown/JSON-shaped tables; cross-check against a reference dict."""

    def score_table(self, prompt: str, generated_table: TableLike, gate: Any) -> Dict[str, Any]:
        rows = self._normalize_rows(generated_table)
        struct = self._structure_sigma(rows)
        vals = self._values_sigma(rows)
        cons = self._consistency_sigma(rows)
        text = json.dumps(rows, default=str)[:6000]
        s_gate, v_gate = gate.score(str(prompt), text)
        comb = 0.25 * struct + 0.25 * vals + 0.2 * cons + 0.3 * float(s_gate)
        return {
            "sigma_structure": round(float(struct), 6),
            "sigma_values": round(float(vals), 6),
            "sigma_consistency": round(float(cons), 6),
            "sigma_gate": round(float(s_gate), 6),
            "verdict_gate": str(v_gate),
            "sigma_combined": round(float(comb), 6),
            "row_count": len(rows),
        }

    @staticmethod
    def _normalize_rows(table: TableLike) -> List[Dict[str, Any]]:
        if isinstance(table, list):
            return [dict(r) for r in table]
        s = str(table).strip()
        if not s:
            return []
        if s.startswith("[") or s.startswith("{"):
            try:
                j = json.loads(s)
                if isinstance(j, list):
                    return [dict(r) for r in j if isinstance(r, Mapping)]
            except json.JSONDecodeError:
                pass
        return SigmaTable._parse_markdown_table(s)

    @staticmethod
    def _parse_markdown_table(md: str) -> List[Dict[str, Any]]:
        lines = [ln.strip() for ln in md.splitlines() if ln.strip().startswith("|")]
        if len(lines) < 2:
            return []
        header = [c.strip() for c in lines[0].strip("|").split("|")]
        rows: List[Dict[str, Any]] = []
        for ln in lines[2:]:
            if set(ln) <= {"|", "-", " "}:
                continue
            cells = [c.strip() for c in ln.strip("|").split("|")]
            if len(cells) != len(header):
                continue
            rows.append(dict(zip(header, cells)))
        return rows

    @staticmethod
    def _structure_sigma(rows: List[Mapping[str, Any]]) -> float:
        if not rows:
            return 0.95
        keys0 = set(rows[0].keys())
        drift = sum(1 for r in rows if set(r.keys()) != keys0)
        return min(1.0, drift * 0.15 + 0.05)

    @staticmethod
    def _values_sigma(rows: List[Mapping[str, Any]]) -> float:
        if not rows:
            return 0.4
        stress = 0.0
        for r in rows:
            for _k, v in r.items():
                if isinstance(v, str) and re.fullmatch(r"-?\d+(\.\d+)?", v.strip()):
                    x = float(v)
                    if abs(x) > 1e12:
                        stress += 0.05
                if v in ("", None, "null", "NaN"):
                    stress += 0.02
        return min(1.0, stress)

    @staticmethod
    def _consistency_sigma(rows: List[Mapping[str, Any]]) -> float:
        if len(rows) < 2:
            return 0.1
        keys = list(rows[0].keys())
        if len(keys) < 2:
            return 0.1
        a, b = keys[0], keys[1]
        bad = 0
        for r in rows:
            try:
                if str(r.get(a, "")).strip().isdigit() and str(r.get(b, "")).strip().isdigit():
                    if int(r[a]) > int(r[b]):
                        bad += 1
            except (TypeError, ValueError):
                continue
        return min(1.0, bad * 0.2)

    def validate_schema(self, table: TableLike, expected_schema: Mapping[str, str]) -> Dict[str, Any]:
        rows = self._normalize_rows(table)
        if not rows:
            return {"ok": False, "error": "empty"}
        keys = set(rows[0].keys())
        exp = set(expected_schema.keys())
        missing = sorted(exp - keys)
        extra = sorted(keys - exp)
        return {"ok": not missing, "missing_columns": missing, "extra_columns": extra}

    @staticmethod
    def outlier_detection(column: str, values: Sequence[Any]) -> Dict[str, Any]:
        nums: List[float] = []
        for v in values:
            try:
                nums.append(float(v))
            except (TypeError, ValueError):
                continue
        if len(nums) < 3:
            return {"outliers": [], "sigma_column": 0.2}
        med = statistics.median(nums)
        mad = statistics.median(abs(x - med) for x in nums) or 1e-9
        out: List[float] = []
        for x in nums:
            if abs(x - med) / mad > 3.5:
                out.append(x)
        return {"outliers": out, "sigma_column": round(min(1.0, len(out) * 0.15), 6)}

    def cross_reference(
        self,
        table: TableLike,
        knowledge_source: Mapping[str, Any],
    ) -> Dict[str, Any]:
        rows = self._normalize_rows(table)
        mism = 0
        total = 0
        for r in rows:
            for k, v in r.items():
                if k in knowledge_source:
                    total += 1
                    if str(v).strip() != str(knowledge_source[k]).strip():
                        mism += 1
        sigma = mism / total if total else 0.0
        return {"mismatches": mism, "checked_cells": total, "sigma": round(float(sigma), 6)}

    def sql_sigma(self, query: str, result: Any, gate: Any) -> Dict[str, Any]:
        res_text = str(result) if not isinstance(result, str) else result
        blob = f"SQL:\n{query}\nRESULT:\n{res_text[:4000]}"
        s, v = gate.score("sql_result_audit", blob)
        inj = bool(re.search(r"(?i)(\bDROP\b|\bDELETE\b|--\s|;.*\bEXEC\b)", query))
        bump = 0.35 if inj else 0.0
        return {
            "sigma": round(min(1.0, float(s) + bump), 6),
            "verdict": str(v),
            "suspected_injection_pattern": inj,
        }
