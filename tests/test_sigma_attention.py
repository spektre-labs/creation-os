# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for σ-attention lab (Python + CLI smoke)."""
from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.sigma_attention import LabTokenSigmaGate, SigmaAttention, benchmark_sigma_vs_dense, kv_prune_rows  # noqa: E402


def test_sigma_attention_full_plus_pruned_counts() -> None:
    n = 16
    d = 8
    gate = LabTokenSigmaGate(n, low_fraction=0.25, low_sigma=0.05, high_sigma=0.8)
    q = [[float((i + k) % 5) / 5.0 for k in range(d)] for i in range(n)]
    k = [list(row) for row in q]
    v = [[x * 0.9 for x in row] for row in q]
    attn = SigmaAttention(gate, window_size=4, sigma_threshold=0.3)
    attn.forward(q, k, v)
    assert attn.stats["full"] + attn.stats["pruned"] == n
    assert attn.stats["full"] >= 1


def test_kv_prune_shrinks_cache() -> None:
    kv = [[float(j) for j in range(8)] for _ in range(10)]
    sig = [0.1 * i for i in range(10)]
    (kv2, s2), n = kv_prune_rows(kv, sig, max_cache=6)
    assert n == 1
    assert len(kv2) == 9
    assert len(s2) == 9


def test_benchmark_large_seq_speedup() -> None:
    out = benchmark_sigma_vs_dense(
        768,
        32,
        window=96,
        threshold=0.3,
        low_fraction=0.12,
        repeats=1,
    )
    assert out["pruned_token_ratio"] > 0.85
    assert out["speedup"] >= 1.8
    assert out["sparse_ms_per_run"] <= out["dense_ms_per_run"] + 1e-6


def test_cos_attention_cli_smoke() -> None:
    env = {**__import__("os").environ, "PYTHONPATH": str(_REPO / "python")}
    r = subprocess.run(
        [
            sys.executable,
            "-m",
            "cos",
            "attention",
            "--input",
            "alpha beta " * 40,
            "--window",
            "32",
            "--threshold",
            "0.35",
        ],
        cwd=str(_REPO),
        env=env,
        capture_output=True,
        text=True,
        check=False,
    )
    assert r.returncode == 0, r.stderr
    body = json.loads(r.stdout.strip())
    assert "full_tokens" in body and "pruned_tokens" in body
