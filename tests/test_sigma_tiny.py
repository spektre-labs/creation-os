# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""v160 σ-tiny CLI smoke."""
from __future__ import annotations

import json
import os
import subprocess
import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]


def test_cos_tiny_footprint_cli() -> None:
    env = {**os.environ, "PYTHONPATH": str(_REPO / "python")}
    r = subprocess.run(
        [sys.executable, "-m", "cos", "tiny", "--footprint"],
        cwd=str(_REPO),
        env=env,
        capture_output=True,
        text=True,
        check=False,
    )
    assert r.returncode == 0, r.stderr
    body = json.loads(r.stdout.strip())
    assert body.get("sigma_state_ram_bytes") == 12


def test_cos_tiny_sensor_cli() -> None:
    env = {**os.environ, "PYTHONPATH": str(_REPO / "python")}
    r = subprocess.run(
        [
            sys.executable,
            "-m",
            "cos",
            "tiny",
            "--sensor",
            "--baseline",
            "2200",
            "--tolerance",
            "500",
            "--reading",
            "2230",
        ],
        cwd=str(_REPO),
        env=env,
        capture_output=True,
        text=True,
        check=False,
    )
    assert r.returncode == 0, r.stderr
    body = json.loads(r.stdout.strip())
    assert body.get("verdict") in ("RETHINK", "ACCEPT", "ABSTAIN")
