# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Sanity checks for Docker one-click Python σ-serve assets (no Docker daemon required)."""
from __future__ import annotations

from pathlib import Path


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def test_dockerfile_exists() -> None:
    root = _repo_root()
    df = root / "Dockerfile.python-oneclick"
    assert df.is_file()
    text = df.read_text(encoding="utf-8")
    assert "python:3.12-slim" in text
    assert "cos" in text and "serve" in text
    assert "8000" in text


def test_compose_exists() -> None:
    root = _repo_root()
    cf = root / "docker-compose.python-oneclick.yml"
    assert cf.is_file()
    text = cf.read_text(encoding="utf-8")
    assert "sigma-gate:" in text
    assert "Dockerfile.python-oneclick" in text
    assert "8000:8000" in text


def test_health_endpoint_defined() -> None:
    root = _repo_root()
    df = (root / "Dockerfile.python-oneclick").read_text(encoding="utf-8")
    yml = (root / "docker-compose.python-oneclick.yml").read_text(encoding="utf-8")
    assert "/health" in df
    assert "/health" in yml
