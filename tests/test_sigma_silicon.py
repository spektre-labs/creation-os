# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""v159 σ-silicon lab smoke tests."""
from __future__ import annotations

import json
import os
import subprocess
import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]


def test_benchmark_targets_json() -> None:
    sys.path.insert(0, str(_REPO / "python"))
    from cos.sigma_silicon import benchmark_targets_json

    j = benchmark_targets_json()
    assert j["evidence_class"]
    assert "hardware_target_cycles" in j


def test_cos_silicon_cli_benchmark() -> None:
    env = {**os.environ, "PYTHONPATH": str(_REPO / "python")}
    r = subprocess.run(
        [sys.executable, "-m", "cos", "silicon", "--benchmark"],
        cwd=str(_REPO),
        env=env,
        capture_output=True,
        text=True,
        check=False,
    )
    assert r.returncode == 0, r.stderr
    body = json.loads(r.stdout.strip())
    assert body.get("hardware_target_cycles")


def test_cos_silicon_cli_semantic_sim() -> None:
    env = {**os.environ, "PYTHONPATH": str(_REPO / "python")}
    r = subprocess.run(
        [
            sys.executable,
            "-m",
            "cos",
            "silicon",
            "--simulate",
            "--test",
            "sigma_update 0x2000 0xF000",
        ],
        cwd=str(_REPO),
        env=env,
        capture_output=True,
        text=True,
        check=False,
    )
    assert r.returncode == 0, r.stderr
    body = json.loads(r.stdout.strip())
    assert body.get("ok") is True
    assert "verdict" in body
