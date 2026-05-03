# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for ``cos.prefill``."""
from __future__ import annotations

from cos.prefill import SigmaPrefill
from cos.sigma_gate import SigmaGate


def test_disaggregate_labels() -> None:
    p = SigmaPrefill()
    d = p.disaggregate({"prefill_gpu": "p0", "decode_gpu": "d1"})
    assert d["prefill_gpu"] == "p0" and d["decode_gpu"] == "d1"


def test_chunked_prefill_split() -> None:
    p = SigmaPrefill()
    chunks = p.chunked_prefill("abcdefghij" * 5, chunk_size=10)
    assert len(chunks) == 5 and all(len(c) == 10 for c in chunks)


def test_sigma_after_prefill_with_gate() -> None:
    p = SigmaPrefill()
    g = SigmaGate()
    out = p.sigma_after_prefill([[0.1, 0.1], [0.2, 0.2]], gate=g, prompt="ctx here")
    assert "sigma_combined" in out and out["gate_verdict"] is not None


def test_prefill_cache_second_hit() -> None:
    p = SigmaPrefill()
    g = SigmaGate()
    a = p.prefill_cache("shared prefix", g)
    b = p.prefill_cache("shared prefix", g)
    assert a["hit"] is False and b["hit"] is True
    assert a["sigma"] == b["sigma"]


def test_ttft_optimization_schema() -> None:
    r = SigmaPrefill.ttft_optimization(max_gate_ms=4.0, use_prefix_cache=False)
    assert r["max_gate_ms"] == 4.0 and r["use_prefix_cache"] is False
