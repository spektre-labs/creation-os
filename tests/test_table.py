# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.table`."""
from __future__ import annotations

from cos.sigma_gate import SigmaGate
from cos.table import SigmaTable


def test_score_table_json_rows() -> None:
    t = SigmaTable()
    rows = [{"a": "1", "b": "2"}, {"a": "3", "b": "4"}]
    r = t.score_table("tab", rows, SigmaGate())
    assert r["row_count"] == 2


def test_validate_schema():
    t = SigmaTable()
    rows = [{"x": 1, "y": 2}]
    v = t.validate_schema(rows, {"x": "int", "y": "int"})
    assert v["ok"] is True


def test_outlier_detection() -> None:
    o = SigmaTable.outlier_detection("c", [1.0, 1.1, 1.2, 100.0])
    assert "outliers" in o


def test_cross_reference() -> None:
    t = SigmaTable()
    r = t.cross_reference([{"sku": "A", "qty": "1"}], {"sku": "A", "qty": "1"})
    assert r["mismatches"] == 0


def test_sql_sigma_injection_hint() -> None:
    t = SigmaTable()
    r = t.sql_sigma("DROP TABLE x;", "{}", SigmaGate())
    assert r["suspected_injection_pattern"] is True
