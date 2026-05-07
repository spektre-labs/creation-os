# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for ``cos mega`` Fabric integration demo."""
from __future__ import annotations

import sys
from pathlib import Path

import pytest

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.fabric import FABRIC_LAYER_MAP, Fabric  # noqa: E402
from cos.zkp import SigmaConstitution  # noqa: E402


@pytest.fixture(scope="module")
def booted_fabric() -> Fabric:
    fab = Fabric()
    fab.boot()
    return fab


def test_mega_boots_and_processes(booted_fabric: Fabric) -> None:
    body = booted_fabric.process("hello mega lab")
    assert "verdict" in body
    assert "σ" in body or "sigma" in body


def test_mega_constitution_check(booted_fabric: Fabric) -> None:
    r = SigmaConstitution().check(booted_fabric.gate)
    assert r["compliant"] is True
    assert r["rules"] == 10


def test_mega_layer_status(booted_fabric: Fabric) -> None:
    layers = booted_fabric.layer_status()
    assert set(layers.keys()) == set(FABRIC_LAYER_MAP.keys())
    for row in layers.values():
        assert "coverage" in row
