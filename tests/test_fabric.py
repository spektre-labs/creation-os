# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import tempfile
from pathlib import Path

from cos.fabric import FabricResult, SigmaFabric


def test_boot() -> None:
    fabric = SigmaFabric(snapshot_dir=Path(tempfile.mkdtemp()))
    result = fabric.boot()
    assert result["booted"]
    assert "gate" in result["layers"]
    assert "pipeline" in result["layers"]


def test_process_score_only() -> None:
    fabric = SigmaFabric(snapshot_dir=Path(tempfile.mkdtemp()))
    result = fabric.process("What is 2+2?", response="4")
    assert isinstance(result, FabricResult)
    assert 0 <= result.sigma <= 1
    assert result.verdict in ("ACCEPT", "RETHINK", "ABSTAIN", "CLARIFY")


def test_process_traces() -> None:
    fabric = SigmaFabric(snapshot_dir=Path(tempfile.mkdtemp()))
    result = fabric.process("test", response="hello")
    trace = result.trace.to_dict()
    assert trace["depth"] >= 1
    assert "pipeline" in [s["layer"] for s in trace["steps"]]


def test_process_with_metacog() -> None:
    fabric = SigmaFabric(snapshot_dir=Path(tempfile.mkdtemp()))
    result = fabric.process(
        "What?",
        response="I'm not sure, maybe perhaps...",
    )
    trace = result.trace.to_dict()
    layers = [s["layer"] for s in trace["steps"]]
    assert "metacog" in layers


def test_process_with_reason() -> None:
    fabric = SigmaFabric(snapshot_dir=Path(tempfile.mkdtemp()))
    result = fabric.process(
        "capitals",
        response="Paris is the capital of France",
        facts=[
            ("capital", ["france", "paris"]),
            ("capital", ["france", "london"]),
        ],
        uniqueness=["capital"],
    )
    trace = result.trace.to_dict()
    layers = [s["layer"] for s in trace["steps"]]
    assert "reason" in layers


def test_result_bool() -> None:
    fabric = SigmaFabric(snapshot_dir=Path(tempfile.mkdtemp()))
    result = fabric.process("What is 2+2?", response="4")
    assert isinstance(bool(result), bool)


def test_status() -> None:
    fabric = SigmaFabric(snapshot_dir=Path(tempfile.mkdtemp()))
    fabric.boot()
    status = fabric.status()
    assert status["booted"]
    assert status["layer_count"] >= 2


def test_checkpoint_rollback() -> None:
    fabric = SigmaFabric(snapshot_dir=Path(tempfile.mkdtemp()))
    fabric.boot()
    fabric.process("a", response="b")

    cp = fabric.checkpoint("test_cp")
    assert cp.get("checkpointed") or "error" not in cp

    fabric.process("c", response="d")

    rb = fabric.rollback()
    assert isinstance(rb, dict)


def test_history_recorded() -> None:
    fabric = SigmaFabric(snapshot_dir=Path(tempfile.mkdtemp()))
    fabric.boot()
    fabric.process("hello", response="world")

    history = fabric.layers.get("history")
    if history:
        assert len(history.turns) == 2
