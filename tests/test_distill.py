# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from cos.distill import SigmaDistill
from cos.sigma_gate import SigmaGate


def test_distill_step_flags_high_sigma() -> None:
    d = SigmaDistill()
    gate = SigmaGate()
    batch = [
        {"prompt": "q", "student_text": "x" * 120},
    ]
    out = d.distill_step(None, None, batch, gate)
    assert out["batch_size"] == 1
    assert "full_kd" in out["rows"][0]


def test_selective_tokens() -> None:
    d = SigmaDistill()
    gate = SigmaGate()
    tlog = [1.0, 0.1, 2.0]
    slog = [0.2, 0.1, 0.0]
    sel = d.selective_tokens(tlog, slog, gate)
    assert isinstance(sel["need_help"], list)


def test_curriculum_order() -> None:
    d = SigmaDistill()
    order = d.sigma_curriculum([0.9, 0.1, 0.5])
    assert order[0] in (1,)


def test_efficiency() -> None:
    d = SigmaDistill()
    eff = d.efficiency({"kd_fraction": 0.2})
    assert eff["skip_fraction"] == 0.8


def test_empty_batch() -> None:
    d = SigmaDistill()
    gate = SigmaGate()
    out = d.distill_step(None, None, [], gate)
    assert out["batch_size"] == 0
    assert out["kd_fraction"] == 0.0


def test_gate_integration() -> None:
    d = SigmaDistill()
    gate = SigmaGate()
    out = d.distill_step(
        None,
        None,
        [{"prompt": "2+2", "student_text": "4"}],
        gate,
    )
    assert "sigma" in out["rows"][0]
