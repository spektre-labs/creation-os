# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from cos.sigma_gate import SigmaGate
from cos.spike import SigmaSpike


def test_lif_spike_threshold() -> None:
    spk, m = SigmaSpike.lif_step(0.0, 2.0, threshold=1.0, decay=0.9)
    assert spk == 1.0
    assert m == 0.0


def test_lif_decay() -> None:
    _, m = SigmaSpike.lif_step(0.5, 0.1, threshold=5.0, decay=0.5)
    assert 0.3 < m < 0.4


def test_spike_attention_add_only() -> None:
    out = SigmaSpike.spike_attention([1, 0, 1], [1, 1, 0], [0.5, 1.0, 2.0])
    assert out == 0.5


def test_convert_gate() -> None:
    gate = SigmaGate()
    cfg = SigmaSpike.convert_gate_to_spike(gate)
    assert "threshold" in cfg
    assert cfg["threshold"] == gate.threshold_accept


def test_energy_estimate_bounds() -> None:
    e = SigmaSpike.energy_estimate(5, 10)
    assert 0.0 <= e["spike_ratio"] <= 1.0
    assert e["relative_energy"] > 0


def test_spike_train_to_sigma() -> None:
    s = SigmaSpike.spike_train_to_sigma([0.0, 1.0, 1.0, 0.0])
    assert 0.0 <= s <= 1.0
