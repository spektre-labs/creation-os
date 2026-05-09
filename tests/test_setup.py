# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""Tests for :mod:`cos.setup` (no Ollama pull in CI)."""
from __future__ import annotations

import io
import json
from contextlib import redirect_stdout

from cos.setup import detect_hardware, has_ollama, pick_model, setup


def test_detect_hardware_keys() -> None:
    hw = detect_hardware()
    for key in ("gpu", "vram_gb", "ram_gb", "os", "arch", "tier"):
        assert key in hw
    assert isinstance(hw["ram_gb"], (int, float))
    assert hw["ram_gb"] > 0


def test_pick_model_tiers() -> None:
    big = pick_model({"vram_gb": 24.0, "ram_gb": 16.0})
    assert big["model"] == "qwen3:8b"
    mid = pick_model({"vram_gb": 8.0, "ram_gb": 8.0})
    assert mid["model"] == "qwen3:4b"
    small = pick_model({"vram_gb": 0.0, "ram_gb": 2.0})
    assert small["model"] == "smollm2:135m"


def test_has_ollama_returns_bool() -> None:
    assert isinstance(has_ollama(), bool)


def test_setup_json_skip_network_skip_demo() -> None:
    buf = io.StringIO()
    with redirect_stdout(buf):
        rc = setup(skip_ollama_pull=True, skip_demo=True, json_out=True)
    assert rc == 0
    data = json.loads(buf.getvalue().strip())
    assert "hardware" in data
    assert "model_plan" in data
    assert "boot" in data
    assert data["demo"]["skipped"] is True
    assert data["ollama_pull"]["status"] in ("skipped", "skipped_by_flag", "ollama_missing")
