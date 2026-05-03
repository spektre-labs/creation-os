# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Structured output + σ-gate — schema checks (lab) plus content scoring.

Grammar / XGrammar-speed claims are **not** implemented here; this layer validates JSON or
dict-shaped outputs and optionally keys off :class:`cos.sigma_gate.SigmaGate`. Valid JSON with
hallucinated fields is flagged by **high σ** even when ``structure_ok`` is true — see
``docs/CLAIM_DISCIPLINE.md``.
"""
from __future__ import annotations

import json
from typing import Any, Dict, List, Mapping, Tuple, Type, Union

__all__ = ["SigmaStruct"]


class SigmaStruct:
    """JSON-schema-shaped validation (subset) + gate scoring + retry helper."""

    def validate_structure(self, output: Union[str, bytes, Mapping[str, Any]], schema: Mapping[str, Any]) -> Dict[str, Any]:
        """Return ``{"ok": bool, "errors": [...]}``; does not raise on failure."""
        errors: List[str] = []
        try:
            if isinstance(output, (str, bytes)):
                obj = json.loads(output)
            else:
                obj = dict(output)
        except (json.JSONDecodeError, TypeError, ValueError) as e:
            return {"ok": False, "errors": [f"json_parse:{e}"], "parsed": None}

        ok = self._check_schema(obj, schema, errors, "$")
        return {"ok": ok and len(errors) == 0, "errors": errors, "parsed": obj}

    def _check_schema(self, value: Any, schema: Mapping[str, Any], errors: List[str], path: str) -> bool:
        ok = True
        st = schema.get("type")
        if st == "object" and not isinstance(value, dict):
            errors.append(f"{path}: expected object")
            return False
        if st == "array" and not isinstance(value, list):
            errors.append(f"{path}: expected array")
            return False
        if st == "string" and not isinstance(value, str):
            errors.append(f"{path}: expected string")
            return False
        if st == "integer" and not isinstance(value, int):
            errors.append(f"{path}: expected integer")
            return False
        if st == "number" and not isinstance(value, (int, float)):
            errors.append(f"{path}: expected number")
            return False

        if isinstance(value, dict) and "properties" in schema:
            req = schema.get("required") or []
            for key in req:
                if key not in value:
                    errors.append(f"{path}: missing required {key!r}")
                    ok = False
            for key, subschema in schema.get("properties", {}).items():
                if key not in value:
                    continue
                if not isinstance(subschema, dict):
                    continue
                if not self._check_schema(value[key], subschema, errors, f"{path}.{key}"):
                    ok = False

        if isinstance(value, list) and "items" in schema:
            items = schema["items"]
            if isinstance(items, dict):
                for i, item in enumerate(value):
                    if not self._check_schema(item, items, errors, f"{path}[{i}]"):
                        ok = False

        return ok

    def validate_content(self, output: str, prompt: str, gate: Any) -> Tuple[float, str]:
        """σ for semantic stress of ``output`` given ``prompt``."""
        return gate.score(str(prompt), str(output))

    def combined_score(
        self,
        output: Union[str, Mapping[str, Any]],
        schema: Mapping[str, Any],
        prompt: str,
        gate: Any,
    ) -> Dict[str, Any]:
        """Both structure and σ must pass policy (caller interprets ``ok``)."""
        vr = self.validate_structure(output, schema)
        text = json.dumps(vr.get("parsed"), ensure_ascii=False) if vr.get("parsed") is not None else str(output)
        sigma, verdict = self.validate_content(text, prompt, gate)
        struct_ok = bool(vr.get("ok"))
        content_ok = verdict == "ACCEPT"
        return {
            "structure_ok": struct_ok,
            "structure_errors": vr.get("errors", []),
            "sigma": float(sigma),
            "verdict": str(verdict),
            "content_ok": content_ok,
            "ok": struct_ok and content_ok,
        }

    @staticmethod
    def schema_from_pydantic(model_class: Type[Any]) -> Dict[str, Any]:
        """Derive JSON Schema from a Pydantic v2 model (optional dependency)."""
        try:
            from pydantic import TypeAdapter
        except ImportError as e:  # pragma: no cover
            raise ImportError("schema_from_pydantic requires pydantic>=2") from e
        return TypeAdapter(model_class).json_schema()

    def constrained_generate(
        self,
        prompt: str,
        schema: Mapping[str, Any],
        model: Any,
        gate: Any,
    ) -> Dict[str, Any]:
        """Call ``model.generate(prompt)``, validate structure, then σ-gate the text."""
        gen = getattr(model, "generate", None)
        if not callable(gen):
            raise TypeError("model must provide generate(prompt) -> str")
        raw = str(gen(prompt))
        vr = self.validate_structure(raw, schema)
        text = raw
        parsed: Any = vr.get("parsed")
        if parsed is not None:
            text = json.dumps(parsed, ensure_ascii=False)
        sigma, verdict = gate.score(prompt, text)
        return {
            "text": raw,
            "structure": vr,
            "sigma": float(sigma),
            "verdict": str(verdict),
        }

    def retry_on_invalid(
        self,
        prompt: str,
        schema: Mapping[str, Any],
        model: Any,
        gate: Any,
        *,
        max_retries: int = 3,
    ) -> Dict[str, Any]:
        """Retry generation until structure ok or attempts exhausted (last attempt returned)."""
        last: Dict[str, Any] = {}
        for attempt in range(max(1, int(max_retries))):
            last = self.constrained_generate(prompt, schema, model, gate)
            last["attempt"] = attempt + 1
            if last.get("structure", {}).get("ok"):
                break
        return last
