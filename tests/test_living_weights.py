# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import sys
from pathlib import Path

import pytest

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.living_weights import LivingWeights  # noqa: E402


class _HighSigmaGate:
    def score(self, prompt: str, response: str):  # noqa: ARG002
        return (0.95, "ACCEPT")


class _LowSigmaGate:
    def score(self, prompt: str, response: str):  # noqa: ARG002
        return (0.05, "ACCEPT")


def test_step_returns_sigma() -> None:
    lw = LivingWeights(gate=_LowSigmaGate(), n_weights=5)
    r = lw.step("hello")
    assert "σ" in r and "sigma" in r
    assert r["σ"] < 0.2


def test_reset_high_sigma_weights() -> None:
    lw = LivingWeights(gate=_HighSigmaGate(), n_weights=8, reset_threshold=0.55)
    for _ in range(8):
        lw.step("x")
    assert lw.resets > 0


def test_strengthen_low_sigma_weights() -> None:
    lw = LivingWeights(
        gate=_LowSigmaGate(),
        n_weights=6,
        reset_threshold=0.99,
        strengthen_threshold=0.48,
    )
    lw.step("once")
    assert lw.strengthens > 0


def test_plasticity_report() -> None:
    lw = LivingWeights(gate=_LowSigmaGate(), n_weights=4)
    lw.step("a")
    rep = lw.plasticity_report()
    assert "avg_σ" in rep and "plasticity" in rep
    assert rep["generation"] == 1


def test_plasticity_decreases_without_resets() -> None:
    """More σ mass with a high reset bar yields fewer cumulative resets than an eager bar."""
    g = _HighSigmaGate()
    loose = LivingWeights(gate=g, n_weights=5, reset_threshold=0.95)
    tight = LivingWeights(gate=g, n_weights=5, reset_threshold=0.55)
    for _ in range(12):
        loose.step("z")
        tight.step("z")
    assert tight.resets >= loose.resets


def test_hebbian_updates_in_learning_zone() -> None:
    np = pytest.importorskip("numpy")
    lw = LivingWeights(gate=_LowSigmaGate(), n_weights=10)
    lw.σ_per_weight[:] = np.linspace(0.35, 0.65, lw.n_weights)
    before = float(lw.weights[0])
    lw.hebbian_update(np.ones(lw.n_weights), 1.0, learning_rate=0.05)
    assert float(lw.weights[0]) != before


def test_kernel_vs_firmware_structure() -> None:
    lw = LivingWeights(gate=_LowSigmaGate(), n_weights=3)
    kv = lw.kernel_vs_firmware()
    assert "kernel" in kv and "firmware" in kv and "principle" in kv
    assert kv["firmware"]["type"] == "living_weights"
