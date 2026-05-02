# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-gate unit tests (fixtures + parametrize; no live models)."""
from __future__ import annotations

import pytest

from cos import SigmaGate, __version__
from tests.constants import HALLUCINATION_CASES


def test_package_version() -> None:
    assert __version__ == "0.1.0"


def test_gate_returns_tuple(gate) -> None:
    result = gate.score("test", "response")
    assert isinstance(result, tuple)
    assert len(result) == 2


def test_gate_sigma_range(gate) -> None:
    sigma, _ = gate.score("test", "hello world")
    assert 0.0 <= sigma <= 1.0


def test_gate_verdict_values(gate) -> None:
    _, verdict = gate.score("test", "hello")
    assert verdict in ("ACCEPT", "RETHINK", "ABSTAIN")


def test_gate_empty_response_abstains(gate) -> None:
    sigma, verdict = gate.score("test", "")
    assert sigma == 1.0
    assert verdict == "ABSTAIN"


@pytest.mark.parametrize("prompt,response,expected", HALLUCINATION_CASES)
def test_gate_hallucination_cases(gate, prompt: str, response: str, expected: str) -> None:
    _, verdict = gate.score(prompt, response)
    if expected == "ABSTAIN":
        assert verdict == "ABSTAIN"
    elif expected == "RETHINK":
        assert verdict in ("RETHINK", "ABSTAIN")
    else:
        assert verdict == "ACCEPT"


def test_gate_deterministic(gate) -> None:
    r1 = gate.score("test prompt", "test response")
    r2 = gate.score("test prompt", "test response")
    assert r1[0] == r2[0]
    assert r1[1] == r2[1]


def test_cross_instance_deterministic() -> None:
    g1 = SigmaGate()
    g2 = SigmaGate()
    s1, v1 = g1.score("same", "same")
    s2, v2 = g2.score("same", "same")
    assert s1 == s2
    assert v1 == v2


def test_score_cascade_dict(gate) -> None:
    result = gate.score_cascade("hello", "hi there")
    assert isinstance(result, dict)
    assert "sigma" in result
    assert "verdict" in result
    assert "levels" in result
    assert "L1_entropy" in result["levels"]


def test_pack_measurement_requires_probe() -> None:
    gate = SigmaGate()
    with pytest.raises(NotImplementedError):
        gate.pack_measurement(0.1, "ACCEPT")


def test_custom_thresholds() -> None:
    gate = SigmaGate(threshold_accept=0.1, threshold_abstain=0.5)
    assert gate.threshold_accept == 0.1
    assert gate.threshold_abstain == 0.5
