# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import tempfile
from pathlib import Path

from cos.fabric import Fabric, FabricResult, SigmaFabric


def test_boot_loads_core() -> None:
    f = Fabric()
    st = f.boot()
    assert st["gate"] == "loaded"
    assert st["config"] == "loaded"


def test_boot_gate_always_loaded() -> None:
    f = Fabric()
    f.boot()
    assert f.gate is f.get("gate")


def test_status_shows_all_modules() -> None:
    f = Fabric()
    st = f.boot()
    for name in (
        "gate",
        "config",
        "graph",
        "memory",
        "symbolic",
        "reason",
        "world_model",
        "drive",
        "meta_goal",
        "conscious",
        "ttt",
        "evolve",
        "observe",
        "drift",
        "tool_safety",
        "omega",
    ):
        assert name in st
        assert st[name] in ("loaded", "missing")


def test_process_returns_sigma() -> None:
    f = Fabric()
    f.boot()
    r = f.process("plan review under σ policy")
    assert "sigma" in r or "σ" in r
    assert "verdict" in r


def test_process_without_omega_falls_back() -> None:
    f = Fabric()
    f.boot()
    f._modules["omega"] = None
    r = f.process("fallback path")
    assert "sigma" in r or "σ" in r
    assert "verdict" in r


def test_cognitive_state_snapshot() -> None:
    f = Fabric()
    f.boot()
    snap = f.cognitive_state()
    assert snap["booted"] is True
    assert "modules" in snap
    assert "claim" in snap
    assert "NOT AGI" in snap["claim"].upper()


def test_missing_module_does_not_crash() -> None:
    f = Fabric()
    f.boot()
    f._modules["symbolic"] = None
    snap = f.cognitive_state()
    assert snap["modules"]["symbolic"] == "missing"


def test_boot_idempotent() -> None:
    f = Fabric()
    a = f.boot()
    b = f.boot()
    assert a == b


# --- SigmaFabric (pipeline orchestration) ---------------------------------


def test_sigma_fabric_boot() -> None:
    fabric = SigmaFabric(snapshot_dir=Path(tempfile.mkdtemp()))
    result = fabric.boot()
    assert result["booted"]
    assert "gate" in result["layers"]
    assert "pipeline" in result["layers"]


def test_sigma_fabric_process_score_only() -> None:
    fabric = SigmaFabric(snapshot_dir=Path(tempfile.mkdtemp()))
    result = fabric.process("What is 2+2?", response="4")
    assert isinstance(result, FabricResult)
    assert 0 <= result.sigma <= 1
    assert result.verdict in ("ACCEPT", "RETHINK", "ABSTAIN", "CLARIFY")


def test_sigma_fabric_process_traces() -> None:
    fabric = SigmaFabric(snapshot_dir=Path(tempfile.mkdtemp()))
    result = fabric.process("test", response="hello")
    trace = result.trace.to_dict()
    assert trace["depth"] >= 1
    assert "pipeline" in [s["layer"] for s in trace["steps"]]


def test_sigma_fabric_process_with_metacog() -> None:
    fabric = SigmaFabric(snapshot_dir=Path(tempfile.mkdtemp()))
    result = fabric.process(
        "What?",
        response="I'm not sure, maybe perhaps...",
    )
    trace = result.trace.to_dict()
    layers = [s["layer"] for s in trace["steps"]]
    assert "metacog" in layers


def test_sigma_fabric_process_with_reason() -> None:
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


def test_sigma_fabric_result_bool() -> None:
    fabric = SigmaFabric(snapshot_dir=Path(tempfile.mkdtemp()))
    result = fabric.process("What is 2+2?", response="4")
    assert isinstance(bool(result), bool)


def test_sigma_fabric_status() -> None:
    fabric = SigmaFabric(snapshot_dir=Path(tempfile.mkdtemp()))
    fabric.boot()
    status = fabric.status()
    assert status["booted"]
    assert status["layer_count"] >= 2


def test_sigma_fabric_checkpoint_rollback() -> None:
    fabric = SigmaFabric(snapshot_dir=Path(tempfile.mkdtemp()))
    fabric.boot()
    fabric.process("a", response="b")

    cp = fabric.checkpoint("test_cp")
    assert cp.get("checkpointed") or "error" not in cp

    fabric.process("c", response="d")

    rb = fabric.rollback()
    assert isinstance(rb, dict)


def test_sigma_fabric_history_recorded() -> None:
    fabric = SigmaFabric(snapshot_dir=Path(tempfile.mkdtemp()))
    fabric.boot()
    fabric.process("hello", response="world")

    history = fabric.layers.get("history")
    if history:
        assert len(history.turns) == 2
