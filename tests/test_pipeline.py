# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for ``cos.pipeline`` (fixtures + parametrize)."""
from __future__ import annotations

import pytest

from cos.pipeline import Pipeline, PipelineResult
from tests.constants import CLEAN_INPUTS, INJECTION_CASES


def test_score_only(pipeline) -> None:
    result = pipeline.score("What is 2+2?", "4")
    assert isinstance(result, PipelineResult)
    assert 0 <= result.sigma <= 1
    assert result.verdict in ("ACCEPT", "RETHINK", "ABSTAIN")


def test_empty_input_blocked(pipeline) -> None:
    result = pipeline.run("")
    assert result.verdict == "BLOCKED"


@pytest.mark.parametrize("injection", INJECTION_CASES)
def test_injection_blocked(pipeline, injection: str) -> None:
    result = pipeline.run(injection)
    assert result.verdict == "BLOCKED"


@pytest.mark.parametrize("clean", CLEAN_INPUTS)
def test_clean_input_not_blocked(pipeline, clean: str) -> None:
    result = pipeline.run(clean, response="Valid response here")
    assert result.verdict != "BLOCKED"


def test_result_bool() -> None:
    r = PipelineResult("ok", 0.1, "ACCEPT")
    assert bool(r) is True
    r2 = PipelineResult(None, 0.9, "ABSTAIN")
    assert bool(r2) is False


def test_result_str() -> None:
    r = PipelineResult("hello", 0.1, "ACCEPT")
    assert str(r) == "hello"


def test_result_dict() -> None:
    r = PipelineResult("test", 0.2, "RETHINK")
    d = r.to_dict()
    assert d["sigma"] == 0.2
    assert d["verdict"] == "RETHINK"


def test_stats_update(pipeline) -> None:
    pipeline.score("a", "b")
    pipeline.score("c", "d")
    stats = pipeline.stats.to_dict()
    assert stats["total"] == 2


def test_no_model_returns_error(pipeline) -> None:
    result = pipeline.run("What is AI?")
    assert result.verdict == "NO_MODEL"


def test_pii_output_blocked_with_forced_accept() -> None:
    class AlwaysAcceptGate:
        def score(self, prompt: str, response: str) -> tuple[float, str]:
            del prompt, response
            return 0.05, "ACCEPT"

    pipe = Pipeline(gate=AlwaysAcceptGate())
    result = pipe.score("test", "My SSN is 123-45-6789")
    assert result.verdict == "OUTPUT_BLOCKED"
    assert result.reason is not None
    assert "PII" in result.reason or "ssn" in result.reason.lower()
