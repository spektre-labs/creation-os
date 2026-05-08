# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.structured` (schema syntax + σ semantics)."""
from __future__ import annotations

from cos import SigmaGate
from cos.structured import SigmaStructured


def _loose_gate() -> SigmaGate:
    return SigmaGate(threshold_accept=0.30, threshold_abstain=0.90)


def test_validate_valid_json() -> None:
    ss = SigmaStructured(gate=_loose_gate())
    schema = {
        "required": ["answer"],
        "properties": {"answer": {"type": "string"}},
    }
    r = ss.validate('{"answer": "4"}', schema, context="What is 2+2?")
    assert r["schema_valid"] is True
    assert r["σ_valid"] is True
    assert r["valid"] is True
    assert r["parsed"] == {"answer": "4"}
    assert r["verdict"] == "ACCEPT"
    assert r["sigma"] == r["σ"]


def test_validate_invalid_json() -> None:
    ss = SigmaStructured()
    schema = {"required": ["x"], "properties": {"x": {"type": "string"}}}
    r = ss.validate("{not json", schema)
    assert r["valid"] is False
    assert r["schema_valid"] is False
    assert "error" in r
    assert "JSON parse" in str(r["error"])


def test_validate_schema_error() -> None:
    ss = SigmaStructured()
    schema = {
        "required": ["answer"],
        "properties": {"answer": {"type": "string"}},
    }
    r = ss.validate('{"wrong": "key"}', schema, context="ctx")
    assert r["valid"] is False
    assert r["schema_valid"] is False
    assert r.get("schema_errors")
    assert any("missing required" in e for e in r["schema_errors"])


def test_validate_sigma_scoring() -> None:
    ss = SigmaStructured(gate=_loose_gate())
    schema = {"required": ["answer"], "properties": {"answer": {"type": "string"}}}
    r = ss.validate({"answer": "4"}, schema, context="What is 2+2?")
    assert "sigma" in r
    assert "σ" in r
    assert "verdict" in r
    assert isinstance(r["sigma"], float)
    assert 0.0 <= r["sigma"] <= 1.0


def test_extract_fields() -> None:
    ss = SigmaStructured(gate=_loose_gate())
    text = "Report: name: Alice, age: 30 for the record."
    fields = {"name": "string", "age": "number"}
    r = ss.extract(text, fields, context="employee snippet")
    assert r["extracted"]["name"] == "Alice"
    assert r["extracted"]["age"] == 30.0
    assert r["complete"] is True
    assert 0.0 <= r["sigma"] <= 1.0


def test_extract_incomplete() -> None:
    ss = SigmaStructured()
    r = ss.extract("Only name: Carol here.", {"name": "string", "city": "string"})
    assert r["extracted"]["name"] == "Carol"
    assert r["extracted"]["city"] is None
    assert r["complete"] is False


def test_schema_type_checking() -> None:
    ss = SigmaStructured()
    schema = {
        "required": ["flag", "count"],
        "properties": {
            "flag": {"type": "boolean"},
            "count": {"type": "integer"},
        },
    }
    r = ss.validate('{"flag": "yes", "count": 3}', schema)
    assert r["schema_valid"] is False
    assert any("flag" in e for e in r["schema_errors"])
