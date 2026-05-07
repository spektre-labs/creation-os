# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import builtins
import tempfile
from pathlib import Path

import pytest

from cos.fabric import Fabric, FabricResult, SigmaFabric


def _module_entry(st: dict, name: str) -> dict:
    return st["modules"][name]


def test_boot_loads_core() -> None:
    f = Fabric()
    st = f.boot()
    assert _module_entry(st, "gate")["state"] == "loaded"
    assert _module_entry(st, "config")["state"] == "loaded"


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
        "causal",
        "moral",
        "omega",
    ):
        assert name in st["modules"]
        assert _module_entry(st, name)["state"] in ("loaded", "missing", "failed", "disabled")


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
    assert "note" in snap
    assert "CLAIM_DISCIPLINE" in snap["note"]


def test_missing_module_does_not_crash() -> None:
    f = Fabric()
    f.boot()
    f._modules["symbolic"] = None
    snap = f.cognitive_state()
    assert snap["modules"]["symbolic"]["state"] == "missing"


def test_boot_idempotent() -> None:
    f = Fabric()
    a = f.boot()
    b = f.boot()
    assert a == b


def test_disabled_module_skipped() -> None:
    f = Fabric(disabled_modules=["graph"])
    st = f.boot()
    assert _module_entry(st, "graph")["state"] == "disabled"
    assert _module_entry(st, "graph")["error"] is None


def test_failed_module_reports_error(monkeypatch: pytest.MonkeyPatch) -> None:
    class BrokenGraph:
        def __init__(self, *a: object, **k: object) -> None:
            raise RuntimeError("graph init failed")

    monkeypatch.setattr("cos.graph.SigmaGraph", BrokenGraph)
    f = Fabric()
    st = f.boot()
    entry = _module_entry(st, "graph")
    assert entry["state"] == "failed"
    assert entry["error"] is not None
    assert "graph init failed" in entry["error"]
    assert "graph" in f._module_errors


def test_status_distinguishes_missing_vs_failed(monkeypatch: pytest.MonkeyPatch) -> None:
    real_import = builtins.__import__

    def import_no_symbolic(
        name: str,
        globals_arg: dict | None = None,
        locals_arg: dict | None = None,
        fromlist: tuple = (),
        level: int = 0,
    ):
        if name == "cos.symbolic" and fromlist and "SigmaSymbolic" in fromlist:
            raise ImportError("package not available")
        return real_import(name, globals_arg, locals_arg, fromlist, level)

    monkeypatch.setattr(builtins, "__import__", import_no_symbolic)
    f_miss = Fabric()
    st_miss = f_miss.boot()
    sym_miss = _module_entry(st_miss, "symbolic")
    assert sym_miss["state"] == "missing"
    assert sym_miss["error"] is None
    assert "symbolic" not in f_miss._module_errors

    monkeypatch.setattr(builtins, "__import__", real_import)

    class BoomReason:
        def __init__(self, *a: object, **k: object) -> None:
            raise ValueError("reason ctor boom")

    monkeypatch.setattr("cos.reason.SigmaReason", BoomReason)
    f_fail = Fabric()
    st_fail = f_fail.boot()
    re = _module_entry(st_fail, "reason")
    assert re["state"] == "failed"
    assert re["error"] is not None
    assert "boom" in re["error"]


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
