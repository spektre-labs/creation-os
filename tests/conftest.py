# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Shared fixtures: repo ``python/`` on ``sys.path``, gates, pipeline, probes (no live models)."""
from __future__ import annotations

import os
import sys
import tempfile
from pathlib import Path

import pytest

from cos import SigmaGate

_ROOT = Path(__file__).resolve().parent.parent
_PYTHON = str(_ROOT / "python")


@pytest.fixture(scope="session", autouse=True)
def _ensure_repo_pythonpath() -> None:
    if _PYTHON not in sys.path:
        sys.path.insert(0, _PYTHON)


@pytest.fixture(scope="session", autouse=True)
def _isolate_cos_engram_for_tests(tmp_path_factory: pytest.TempPathFactory) -> None:
    """Avoid polluting ``~/.cos/engram.json`` during ``Fabric.boot()`` / Engram tests."""
    if os.environ.get("COS_ENGRAM_PATH", "").strip():
        yield
        return
    p = tmp_path_factory.mktemp("cos_engram") / "engram.json"
    os.environ["COS_ENGRAM_PATH"] = str(p)
    try:
        yield
    finally:
        os.environ.pop("COS_ENGRAM_PATH", None)


@pytest.fixture
def gate():
    return SigmaGate()


@pytest.fixture
def gate_strict():
    return SigmaGate(threshold_accept=0.1, threshold_abstain=0.5)


@pytest.fixture
def gate_loose():
    return SigmaGate(threshold_accept=0.5, threshold_abstain=0.9)


@pytest.fixture
def sample_triplets():
    return [
        ("Paris", "capital_of", "France"),
        ("Berlin", "capital_of", "Germany"),
        ("Tokyo", "capital_of", "Japan"),
    ]


@pytest.fixture
def sample_prompts():
    return [
        ("What is 2+2?", "4"),
        ("Who discovered radium?", "Marie Curie"),
        ("What is the capital of France?", "Paris"),
    ]


@pytest.fixture
def pipeline():
    from cos.pipeline import Pipeline

    return Pipeline()


@pytest.fixture
def cascade():
    from cos.probe import SignalCascade

    return SignalCascade()


@pytest.fixture
def stream():
    from cos.stream import SigmaStream

    return SigmaStream()


@pytest.fixture
def calibrator():
    from cos.calibrate import SigmaCalibrator

    return SigmaCalibrator(method="platt")


@pytest.fixture
def history():
    from cos.snapshot import ConversationHistory

    return ConversationHistory()


@pytest.fixture
def snapshot_mgr(pipeline):
    from cos.snapshot import SnapshotManager

    return SnapshotManager(pipeline, snapshot_dir=tempfile.mkdtemp())


@pytest.fixture
def tmp_dir():
    with tempfile.TemporaryDirectory() as d:
        yield d

