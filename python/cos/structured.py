# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Structured output with σ-validation (no third-party deps).

Schema-style checks enforce **syntax** (shapes and required keys). :class:`~cos.sigma_gate.SigmaGate`
scores **semantics** (whether the structured content fits the stated context).

Overall ``valid`` is true only when the payload is JSON object that passes schema checks **and**
the σ-gate returns ``ACCEPT``.
"""
from __future__ import annotations

import json
from typing import Any, Dict, List, Mapping, Union

from cos.sigma_gate import SigmaGate

JsonLike = Union[str, bytes, bytearray, Mapping[str, Any]]


class SigmaStructured:
    """Structured output: schema-valid **and** σ-scored."""

    def __init__(self, gate: Any = None) -> None:
        self.gate = gate or SigmaGate()

    def validate(
        self,
        output: JsonLike,
        schema: Mapping[str, Any],
        context: str = "",
    ) -> Dict[str, Any]:
        """Validate structured output: JSON parse, schema, then σ-gate.

        ``output`` is a JSON string or mapping. ``schema`` is a loose JSON-Schema-shaped
        dict with ``required`` and ``properties`` (see :meth:`_check_schema`).
        """
        if isinstance(output, (bytes, bytearray)):
            output = output.decode("utf-8", errors="replace")

        if isinstance(output, str):
            try:
                parsed: Any = json.loads(output)
            except json.JSONDecodeError as e:
                return {
                    "valid": False,
                    "sigma": 1.0,
                    "σ": 1.0,
                    "verdict": "ABSTAIN",
                    "error": f"JSON parse error: {e}",
                    "parsed": None,
                    "schema_valid": False,
                    "σ_valid": False,
                }
        else:
            parsed = dict(output) if isinstance(output, Mapping) else output

        if not isinstance(parsed, dict):
            return {
                "valid": False,
                "sigma": 1.0,
                "σ": 1.0,
                "verdict": "ABSTAIN",
                "error": "expected JSON object at top level",
                "parsed": parsed,
                "schema_valid": False,
                "σ_valid": False,
            }

        schema_errors = self._check_schema(parsed, schema)
        if schema_errors:
            return {
                "valid": False,
                "sigma": 0.9,
                "σ": 0.9,
                "verdict": "ABSTAIN",
                "schema_errors": schema_errors,
                "parsed": parsed,
                "schema_valid": False,
                "σ_valid": False,
            }

        prompt = context.strip() if context else json.dumps(schema, sort_keys=True)
        response_blob = json.dumps(parsed, sort_keys=True)
        sigma, verdict = self.gate.score(prompt, response_blob)
        sigma_f = round(float(sigma), 4)
        verdict_s = str(verdict)
        sigma_ok = verdict_s == "ACCEPT"

        return {
            "valid": True if sigma_ok else False,
            "sigma": sigma_f,
            "σ": sigma_f,
            "verdict": verdict_s,
            "parsed": parsed,
            "schema_valid": True,
            "σ_valid": sigma_ok,
        }

    def extract(
        self,
        text: str,
        fields: Mapping[str, str],
        context: str = "",
    ) -> Dict[str, Any]:
        """Heuristic field extraction from unstructured text (no LLM)."""
        extracted: Dict[str, Any] = {}
        for field, ftype in fields.items():
            extracted[field] = self._extract_field(text, field, ftype)

        prompt = (context or text).strip()
        blob = json.dumps(extracted, sort_keys=True)
        sigma, verdict = self.gate.score(prompt, blob)
        sigma_f = round(float(sigma), 4)
        return {
            "extracted": extracted,
            "sigma": sigma_f,
            "σ": sigma_f,
            "verdict": str(verdict),
            "complete": all(v is not None for v in extracted.values()),
        }

    def _check_schema(self, data: Mapping[str, Any], schema: Mapping[str, Any]) -> List[str]:
        """Subset of JSON Schema: ``required`` + ``properties`` with ``type``."""
        errors: List[str] = []
        required = list(schema.get("required") or [])
        properties = dict(schema.get("properties") or {})

        for field in required:
            if field not in data:
                errors.append(f"missing required field: {field}")

        for field, spec in properties.items():
            if field not in data:
                continue
            if not isinstance(spec, dict):
                continue
            expected_type = str(spec.get("type", "string"))
            actual = data[field]
            if expected_type == "string" and not isinstance(actual, str):
                errors.append(f"{field}: expected string, got {type(actual).__name__}")
            elif expected_type == "number":
                if isinstance(actual, bool) or not isinstance(actual, (int, float)):
                    errors.append(f"{field}: expected number, got {type(actual).__name__}")
            elif expected_type == "integer":
                if isinstance(actual, bool) or not isinstance(actual, int):
                    errors.append(f"{field}: expected integer, got {type(actual).__name__}")
            elif expected_type == "boolean" and not isinstance(actual, bool):
                errors.append(f"{field}: expected boolean, got {type(actual).__name__}")
            elif expected_type == "array" and not isinstance(actual, list):
                errors.append(f"{field}: expected array, got {type(actual).__name__}")

        return errors

    def _extract_field(self, text: str, field: str, ftype: str) -> Any:
        """Heuristic ``field: value`` extraction."""
        if not str(text).strip():
            return None
        text_lower = text.lower()
        field_lower = field.lower()

        for sep in (":", " is ", " = ", " was "):
            needle = field_lower + sep
            if needle not in text_lower:
                continue
            idx = text_lower.index(needle)
            start = idx + len(needle)
            rest = text[start:].strip()
            chunk = rest.split(".")[0].split(",")[0].split("\n")[0].strip()
            if not chunk:
                return None
            if ftype == "number":
                for tok in chunk.split():
                    try:
                        return float(tok)
                    except ValueError:
                        continue
                return None
            if ftype == "integer":
                for tok in chunk.split():
                    try:
                        return int(tok)
                    except ValueError:
                        continue
                return None
            if ftype == "string":
                toks = chunk.split()
                return toks[0] if toks else None
            return chunk
        return None


__all__ = ["SigmaStructured"]
