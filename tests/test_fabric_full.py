# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Full :class:`~cos.fabric.Fabric` pipeline integration — multi-layer trace + gate invariant."""

from __future__ import annotations


def test_full_pipeline_all_layers() -> None:
    """Trace includes the σ-gate and at least one non-skipped companion layer."""
    from cos.fabric import Fabric

    f = Fabric()
    f.boot()
    result = f.process("What causes rain?")
    assert "σ" in result
    assert "verdict" in result
    assert "trace" in result
    assert result["layers_active"] >= 2
    assert "latency_ms" in result


def test_think_trace_has_single_gate_row() -> None:
    """Exactly one ``gate`` row; full stack produces a trace."""
    from cos.fabric import Fabric

    f = Fabric()
    f.boot()
    result = f.process("Is water wet?")
    assert len(result["trace"]) >= 1
    gate_traces = [t for t in result["trace"] if t.get("layer") == "gate"]
    assert len(gate_traces) == 1
    assert "σ" in gate_traces[0]


def test_pipeline_graceful_without_optional_world_model() -> None:
    """Optional modules may be cleared; gate still runs."""
    from cos.fabric import Fabric

    f = Fabric()
    f.boot()
    f._modules["world_model"] = None
    result = f.process("test")
    assert "σ" in result
    assert result["verdict"] in ("ACCEPT", "RETHINK", "ABSTAIN")
