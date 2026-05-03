# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.contamination`."""
from __future__ import annotations

from cos.contamination import SigmaContamination


def test_check_dataset_duplicate_signal() -> None:
    c = SigmaContamination()
    dups = [{"text": "same"} for _ in range(25)]
    r = c.check_dataset(dups, None)
    assert r["duplicate_rows"] > 0


def test_canary_test_echo() -> None:
    c = SigmaContamination()

    class M:
        def generate(self, p: str) -> str:
            return p + " echoed"

    canary = "COS_CANARY_VERIFY_98765xyzzy"
    r = c.canary_test(M(), canary)
    assert r["remembered"] is True


def test_output_distribution_peakedness() -> None:
    c = SigmaContamination()
    ds = [{"output": "x" * (10 + i * 30)} for i in range(8)]
    r = c.output_distribution_peakedness(None, ds)
    assert "peakedness_proxy" in r and "entropy" in r


def test_temporal_filter_cutoff() -> None:
    c = SigmaContamination()
    rows = [
        {"id": 1, "timestamp": "2020-01-01T00:00:00"},
        {"id": 2, "timestamp": "2030-01-01T00:00:00"},
        {"id": 3, "text": "no_ts"},
    ]
    kept, drop = c.temporal_filter(rows, "2025-01-01")
    ids_kept = {r["id"] for r in kept}
    ids_drop = {r["id"] for r in drop}
    assert 1 in ids_drop and 2 in ids_kept and 3 in ids_kept


def test_clean_and_report() -> None:
    c = SigmaContamination()
    ds = [{"text": "u", "id": 1}]
    cl = c.clean_benchmark(ds, None)
    assert "clean" in cl
    rep = c.report(ds, None)
    assert "possibly_contaminated_fraction" in rep
