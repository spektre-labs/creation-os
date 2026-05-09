# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.health` (static ``cos`` tree report)."""
from __future__ import annotations

from pathlib import Path

from cos.health import ProjectHealth


def test_project_health_scan_shape() -> None:
    r = ProjectHealth().scan()
    assert set(r) >= {
        "modules",
        "complexity",
        "connectivity",
        "dead_code",
        "test_coverage",
        "line_count",
        "health_sigma",
        "verdict",
    }
    assert r["modules"] >= 1
    assert r["verdict"] in ("HEALTHY", "NEEDS ATTENTION", "CRITICAL")
    assert 0.0 <= r["health_sigma"] <= 1.0


def test_project_health_custom_root(tmp_path: Path) -> None:
    pkg = tmp_path / "pkg"
    pkg.mkdir()
    (pkg / "mod_a.py").write_text("def sigma_helper():\n    return 1\n", encoding="utf-8")
    r = ProjectHealth(pkg).scan()
    assert r["modules"] == 1
    assert r["line_count"]["total_lines"] >= 2
