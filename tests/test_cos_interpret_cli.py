# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
from __future__ import annotations

import json
import os
import subprocess
import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]


def test_cos_interpret_mock_decompose_json() -> None:
    env = os.environ.copy()
    env["PYTHONPATH"] = str(_REPO / "python")
    r = subprocess.run(
        [sys.executable, "-m", "cos", "interpret", "--mock", "--decompose"],
        cwd=str(_REPO),
        env=env,
        capture_output=True,
        text=True,
        timeout=30,
        check=False,
    )
    assert r.returncode == 0, r.stderr
    obj = json.loads(r.stdout.strip())
    assert obj["op"] == "decompose"
    assert "cascade" in obj
