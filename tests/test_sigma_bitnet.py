# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for v158 σ-BitNet lab (packed ternary matvec + σ stack + ``cos bitnet`` smoke)."""
from __future__ import annotations

import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.sigma_bitnet import (  # noqa: E402
    COS_Q16,
    matvec_packed,
    pack_int8_ternary,
    probe_gguf_header,
    sparsity_packed,
    stack_forward_sigma_gated,
    toy_layers_for_prompt,
)


def test_pack_matvec_matches_naive() -> None:
    w = [-1, 0, 1, 1, 0, -1, 1, 0]
    x = [3, -2, 5, 7, 1, 0, -4, 2]
    pk = pack_int8_ternary(w)
    acc = 0
    for wi, xi in zip(w, x):
        if wi > 0:
            acc += xi
        elif wi < 0:
            acc -= xi
    y = matvec_packed(pk, x, rows=1, cols=8, scale_q16=COS_Q16)
    assert y == [int((acc * COS_Q16) >> 16)]


def test_sparsity_frac() -> None:
    w = [0, 0, 1, -1]
    pk = pack_int8_ternary(w)
    assert abs(sparsity_packed(pk, 4) - 0.5) < 1e-6


def test_stack_abstain_with_zero_k_raw() -> None:
    layers = toy_layers_for_prompt("abstain-probe", dim=4, n_layers=3)
    x0 = [1, -1, 2, 0]
    code, _out, sigs = stack_forward_sigma_gated(layers, x0, k_raw=0.0)
    assert 0 <= code < len(layers)
    assert len(sigs) == code + 1


def test_probe_gguf_header_short_file() -> None:
    with tempfile.NamedTemporaryFile(delete=False) as f:
        f.write(b"ab")
        path = f.name
    try:
        o = probe_gguf_header(path)
        assert o.get("ok") is False
    finally:
        os.unlink(path)


def test_cos_bitnet_cli_sparsity() -> None:
    env = {**os.environ, "PYTHONPATH": str(_REPO / "python")}
    r = subprocess.run(
        [sys.executable, "-m", "cos", "bitnet", "--sparsity"],
        cwd=str(_REPO),
        env=env,
        capture_output=True,
        text=True,
        check=False,
    )
    assert r.returncode == 0, r.stderr
    body = json.loads(r.stdout.strip())
    assert "avg_sparsity" in body


def test_cos_bitnet_cli_prompt_json() -> None:
    env = {**os.environ, "PYTHONPATH": str(_REPO / "python")}
    r = subprocess.run(
        [
            sys.executable,
            "-m",
            "cos",
            "bitnet",
            "--prompt",
            "What is 2+2?",
            "--dim",
            "8",
            "--layers",
            "4",
            "--k-raw",
            "0.92",
        ],
        cwd=str(_REPO),
        env=env,
        capture_output=True,
        text=True,
        check=False,
    )
    assert r.returncode == 0, r.stderr
    body = json.loads(r.stdout.strip())
    assert "code" in body and "layers_run" in body
    assert int(body["layers_run"]) >= 1
