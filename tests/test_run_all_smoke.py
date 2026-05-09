# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Smoke import for ``cos.eval.run_all``."""
from __future__ import annotations

import os
import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

import cos.eval.run_all as run_all  # noqa: E402


def test_repo_root_has_creation_marker() -> None:
    root = run_all.repo_root()
    assert (root / "creation_os_v2.c").is_file()


def test_run_cmd_invokes_python() -> None:
    root = run_all.repo_root()
    r = run_all.run_cmd(
        [sys.executable, "-c", "print('hi')"],
        cwd=root,
        env={**os.environ, "PYTHONWARNINGS": "ignore"},
        timeout=10.0,
    )
    assert r["ok"]
    assert "hi" in r["out"]

