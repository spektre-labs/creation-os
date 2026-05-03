# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""Tests for :mod:`cos.struct`."""
from __future__ import annotations

import pytest

from cos import SigmaGate
from cos.struct import SigmaStruct

_SCHEMA = {
    "type": "object",
    "required": ["answer"],
    "properties": {"answer": {"type": "integer"}},
}


def test_struct_validate_ok() -> None:
    st = SigmaStruct()
    r = st.validate_structure('{"answer": 4}', _SCHEMA)
    assert r["ok"] is True
    assert r["parsed"] == {"answer": 4}


def test_struct_validate_missing_required() -> None:
    st = SigmaStruct()
    r = st.validate_structure('{"wrong": 1}', _SCHEMA)
    assert r["ok"] is False


def test_struct_combined_score_includes_content() -> None:
    st = SigmaStruct()
    gate = SigmaGate()
    c = st.combined_score('{"answer": 4}', _SCHEMA, "2+2?", gate)
    assert "structure_ok" in c and "sigma" in c
    s, v = st.validate_content("4", "2+2?", gate)
    assert v in ("ACCEPT", "RETHINK", "ABSTAIN")


def test_struct_constrained_generate() -> None:
    class M:
        def generate(self, prompt: str) -> str:
            return '{"answer": 4}'

    st = SigmaStruct()
    gate = SigmaGate()
    out = st.constrained_generate("Return JSON", _SCHEMA, M(), gate)
    assert out["structure"]["ok"] is True


def test_struct_retry_on_invalid() -> None:
    class Flaky:
        def __init__(self) -> None:
            self.n = 0

        def generate(self, prompt: str) -> str:
            self.n += 1
            if self.n < 2:
                return "not-json"
            return '{"answer": 4}'

    st = SigmaStruct()
    gate = SigmaGate()
    out = st.retry_on_invalid("x", _SCHEMA, Flaky(), gate, max_retries=3)
    assert out["structure"]["ok"] is True


def test_struct_schema_from_pydantic() -> None:
    pydantic = pytest.importorskip("pydantic")

    class M(pydantic.BaseModel):
        answer: int

    s = SigmaStruct.schema_from_pydantic(M)
    assert s.get("type") == "object"
    assert "properties" in s
