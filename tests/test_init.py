# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.init` and ``cos init`` CLI."""
from __future__ import annotations

from pathlib import Path

from cos.init import bootstrap_project, run_cos_init


def test_bootstrap_creates_layout(tmp_path) -> None:
    bootstrap_project(tmp_path / "proj", "enterprise")
    root = tmp_path / "proj"
    assert (root / "cos_config.yaml").is_file()
    assert (root / "probes").is_dir()
    assert (root / "evals" / "sample.jsonl").is_file()
    assert (root / "examples" / "basic_score.py").is_file()


def test_automotive_offline_in_yaml(tmp_path) -> None:
    bootstrap_project(tmp_path / "car", "automotive")
    yaml = (tmp_path / "car" / "cos_config.yaml").read_text(encoding="utf-8")
    assert "offline: true" in yaml
    assert "threshold_accept: 0.15" in yaml


def test_run_cos_init_cli_style(tmp_path) -> None:
    assert run_cos_init(name="n1", persona="research", dest=tmp_path) == 0
    assert (tmp_path / "n1" / "README.md").is_file()


def test_run_cos_init_errors_if_exists(tmp_path) -> None:
    (tmp_path / "n2").mkdir()
    assert run_cos_init(name="n2", persona="enterprise", dest=tmp_path) == 1
