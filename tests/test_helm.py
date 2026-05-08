# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

"""Sanity checks for the Creation OS Helm chart (no cluster or helm binary required)."""
from __future__ import annotations

from pathlib import Path

import pytest

yaml = pytest.importorskip("yaml")


def _chart_dir() -> Path:
    return Path(__file__).resolve().parents[1] / "deploy" / "helm" / "creation-os"


def test_chart_yaml_exists() -> None:
    chart = _chart_dir() / "Chart.yaml"
    assert chart.is_file()
    raw = chart.read_text(encoding="utf-8")
    assert "apiVersion: v2" in raw
    assert "name: creation-os" in raw
    assert "version:" in raw


def test_values_yaml_valid() -> None:
    vf = _chart_dir() / "values.yaml"
    assert vf.is_file()
    data = yaml.safe_load(vf.read_text(encoding="utf-8"))
    assert isinstance(data, dict)
    assert data.get("replicaCount") == 1
    assert data.get("health", {}).get("path") == "/health"
    assert data.get("autoscaling", {}).get("targetCPUUtilizationPercentage") == 70
    assert data.get("gpu", {}).get("enabled") is False


def test_deployment_template_exists() -> None:
    tpl = _chart_dir() / "templates" / "deployment.yaml"
    assert tpl.is_file()
    text = tpl.read_text(encoding="utf-8")
    assert "kind: Deployment" in text
    assert "/health" in text or "{{ .Values.health.path }}" in text
