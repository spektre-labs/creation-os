# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Shared fixtures: repo ``python/`` on ``sys.path``, gates, pipeline, probes (no live models)."""
from __future__ import annotations

import sys
import tempfile
from pathlib import Path

import pytest

_ROOT = Path(__file__).resolve().parent.parent
_PYTHON = str(_ROOT / "python")


@pytest.fixture(scope="session", autouse=True)
def _ensure_repo_pythonpath() -> None:
    if _PYTHON not in sys.path:
        sys.path.insert(0, _PYTHON)


@pytest.fixture
def gate():
    from cos.sigma_gate import SigmaGate

    return SigmaGate()


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


from tests.constants import CLEAN_INPUTS, HALLUCINATION_CASES, INJECTION_CASES  # noqa: E402


def pytest_configure(config: pytest.Config) -> None:
    config.addinivalue_line("markers", "slow: tests that are intentionally slow")
    config.addinivalue_line("markers", "integration: optional third-party imports (e.g. langchain-core)")
