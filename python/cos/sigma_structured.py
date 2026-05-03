# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
σ-structured — schema-shaped JSON plus semantic σ checks (lab).

``valid JSON ≠ correct JSON``: structure is checked locally; σ scores content.
See ``docs/CLAIM_DISCIPLINE.md`` before treating scores as leaderboard evidence.
"""
from __future__ import annotations

import json
from typing import Any, Dict, List, Optional, Protocol, Tuple, runtime_checkable

from .sigma_gate_core import Verdict


@runtime_checkable
class StructuredGate(Protocol):
    def score(self, prompt: str, text: str) -> Tuple[float, Verdict]:
        ...


@runtime_checkable
class StructuredModel(Protocol):
    def generate(self, prompt: str, *, response_format: Optional[Dict[str, Any]] = None) -> str:
        ...


class SigmaStructured:
    """Generate JSON bound to a small schema dict, then score with σ (whole + per-field)."""

    def __init__(self, gate: StructuredGate) -> None:
        self.gate = gate

    @staticmethod
    def validate_schema(data: Any, schema: Dict[str, Any]) -> bool:
        if schema.get("type") != "object" or not isinstance(data, dict):
            return False
        props = schema.get("properties")
        if not isinstance(props, dict):
            return True
        required: List[str] = list(schema.get("required") or [])
        for key in required:
            if key not in data:
                return False
        for key, spec in props.items():
            if key not in data:
                continue
            if not isinstance(spec, dict):
                continue
            t = spec.get("type")
            val = data[key]
            if t == "string" and not isinstance(val, str):
                return False
            if t == "integer":
                if isinstance(val, bool) or not isinstance(val, int):
                    return False
            elif t == "number":
                if isinstance(val, bool) or not isinstance(val, (int, float)):
                    return False
            elif t == "boolean" and not isinstance(val, bool):
                return False
        return True

    def generate_and_validate(
        self,
        model: StructuredModel,
        prompt: str,
        schema: Dict[str, Any],
    ) -> Dict[str, Any]:
        fmt = {"type": "json_schema", "schema": schema}
        response = model.generate(prompt, response_format=fmt)
        try:
            data = json.loads(response)
        except json.JSONDecodeError:
            return {"valid": False, "reason": "invalid JSON", "sigma": 1.0, "verdict": "ABSTAIN"}

        if not self.validate_schema(data, schema):
            return {"valid": False, "reason": "schema mismatch", "sigma": 0.9, "verdict": "ABSTAIN"}

        sigma, verdict = self.gate.score(prompt, response)
        field_sigmas: Dict[str, float] = {}
        for field, value in data.items():
            f_sigma, _f_v = self.gate.score(
                f"For the question {prompt!r}, is field {field!r} = {value!r} factually correct?",
                f"Yes, {field} = {value} is correct.",
            )
            field_sigmas[str(field)] = float(f_sigma)

        vn = verdict.name if isinstance(verdict, Verdict) else str(verdict)
        return {
            "valid": vn != "ABSTAIN",
            "data": data,
            "sigma": float(sigma),
            "verdict": vn,
            "field_sigmas": field_sigmas,
            "high_sigma_fields": [f for f, s in field_sigmas.items() if s > 0.5],
        }


__all__ = ["SigmaStructured"]
